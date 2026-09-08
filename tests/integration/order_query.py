#!/usr/bin/env python3
"""Real TCP authorization/read-only regression. Usage: order_query.py BUILD_DIR."""
import json
import socket
import sqlite3
import struct
import subprocess
import sys
import tempfile
import time
import uuid
from pathlib import Path


def receive(sock, size):
    data = b''
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            raise AssertionError('Server closed connection')
        data += chunk
    return data


def request(sock, kind, data, version=1):
    request_id = str(uuid.uuid4())
    body = json.dumps(dict(protocol_version=version, request_id=request_id, data=data)).encode()
    sock.sendall(struct.pack('!II', kind, len(body)) + body)
    while True:
        response_kind, length = struct.unpack('!II', receive(sock, 8))
        response = json.loads(receive(sock, length))
        if response_kind != 0x32:  # Charge updates can interleave with replies.
            break
    assert response_kind == {1: 2, 7: 8, 0x10: 0x11, 0x30: 0x31}[kind]
    assert response['request_id'] == request_id
    return response


def run(build):
    with tempfile.TemporaryDirectory(prefix='ev-order-integration-') as directory:
        db_path = Path(directory) / 'test.sqlite3'
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        with open(Path(directory) / 'server.log', 'w+') as log:
            server = subprocess.Popen([str(build / 'bin/ev_server'), '--host', '127.0.0.1',
                                       '--port', str(port), '--database', str(db_path)],
                                      stdout=log, stderr=log)
            try:
                for _ in range(100):
                    try:
                        first = socket.create_connection(('127.0.0.1', port), timeout=2)
                        break
                    except OSError:
                        if server.poll() is not None:
                            log.seek(0)
                            raise AssertionError(log.read())
                        time.sleep(.05)
                else:
                    raise AssertionError('Server startup timed out')
                with first, socket.create_connection(('127.0.0.1', port), timeout=2) as second, \
                        sqlite3.connect(db_path) as db:
                    phone = db.execute('SELECT phone FROM users WHERE user_id=1').fetchone()[0]
                    login = request(first, 1, dict(phone=phone, is_auto_register=False))
                    assert login['success'], login
                    session = login['data']['user_info']['session_id']
                    query = dict(type='query_orders', params={}, session_id=session)
                    # Bind to the connection, not just possession of a token.
                    assert request(second, 0x10, query)['code'] == 'UNAUTHORIZED'
                    assert request(first, 0x10, dict(query, session_id=''))['code'] == 'UNAUTHORIZED'
                    assert request(first, 0x10, query, version=99)['code'] == 'PROTOCOL_ERROR'
                    before = list(db.iterdump())
                    response = request(first, 0x10, dict(query, user_id=2, params={'user_id': 2}))
                    assert response['success'], response
                    orders = response['data']['result']['orders']
                    expected = db.execute('SELECT order_id FROM orders WHERE user_id=1 '
                                          'ORDER BY reserve_time DESC, order_id DESC').fetchall()
                    assert [o['order_id'] for o in orders] == [row[0] for row in expected]
                    assert list(db.iterdump()) == before, 'Query changed persistent state'
                    assert all('duration_hours' in o and 'start_time' in o and 'end_time' in o for o in orders)
                    # Empty account, and no previous account's data after switching login.
                    registered = request(second, 1, dict(phone='19900000999', is_auto_register=True))
                    assert registered['success'], registered
                    empty_session = registered['data']['user_info']['session_id']
                    assert request(second, 0x10, dict(query, session_id=empty_session))['data']['result']['orders'] == []
                    # Charge lifecycle and order-list integration use the same persistent order.
                    def charge(action, **params):
                        result = request(second, 0x30, dict(type=action, params=params, session_id=empty_session))
                        assert result['success'], result
                        return result['data']['result']

                    def current_order(order_id, status):
                        result = request(second, 0x10, dict(query, session_id=empty_session))
                        assert result['success'], result
                        order = next(o for o in result['data']['result']['orders'] if o['order_id'] == order_id)
                        assert order['status'] == status, order
                        assert order['station_name'] and order['pile_number'], order
                        return order

                    pile_id = db.execute("SELECT pile_id FROM charging_piles WHERE status='idle' LIMIT 1").fetchone()[0]
                    order_id = charge('reserve', pile_id=pile_id)['order_id']
                    current_order(order_id, 'reserved')
                    charge('cancel_charge', order_id=order_id)
                    current_order(order_id, 'cancelled')
                    order_id = charge('reserve', pile_id=pile_id)['order_id']
                    charge('start_charge', order_id=order_id)
                    current_order(order_id, 'charging')
                    # Move the isolated test order back two minutes to deterministically accrue a fee.
                    db.execute("UPDATE orders SET start_time=datetime('now','localtime','-2 minutes') WHERE order_id=?", (order_id,))
                    db.commit()
                    outcome = charge('end_charge', order_id=order_id)
                    assert not outcome['settled'], outcome
                    pending = current_order(order_id, 'pending_settlement')
                    assert pending['duration_hours'] > 0
                    fee = pending['total_fee']
                    assert fee > 0
                    recharge = request(second, 0x10, dict(type='recharge', params={'amount_cent': 100000}, session_id=empty_session))
                    assert recharge['success'], recharge
                    outcome = charge('end_charge', order_id=order_id)
                    assert outcome['settled'], outcome
                    settled = current_order(order_id, 'settled')
                    assert settled['total_fee'] == fee
                    assert round(settled['total_fee'] * 100) == outcome['total_fee_cent']
                    before_query = list(db.iterdump())
                    current_order(order_id, 'settled')
                    assert list(db.iterdump()) == before_query
                    # Admin and user primary keys may collide; role must still be enforced.
                    admin = request(second, 1, dict(role='admin', username='admin', password='admin123'))
                    assert admin['success'], admin
                    assert request(second, 0x10, dict(query, session_id=admin['data']['session_id']))['code'] == 'UNAUTHORIZED'
                    db.execute("UPDATE users SET status='frozen' WHERE user_id=1")
                    db.commit()
                    assert request(first, 0x10, query)['code'] == 'ACCOUNT_FROZEN'
                    assert request(first, 0x10, query)['code'] == 'UNAUTHORIZED'
                    assert request(second, 7, dict(session_id=empty_session))['success']
                    assert request(second, 0x10, dict(query, session_id=empty_session))['code'] == 'UNAUTHORIZED'
                print('Order TCP integration passed: isolation, roles, frozen/logout sessions, empty list, sorting, read-only, reserve/cancel/charge/settlement lifecycle.')
            finally:
                server.terminate()
                server.wait(timeout=5)


if __name__ == '__main__':
    run(Path(sys.argv[1]).resolve())
