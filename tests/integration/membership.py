#!/usr/bin/env python3
"""Real Qt server + TCP clients + isolated SQLite + local model protocol fixture.

No external AI calls, real account credentials, payments or API keys are used.
Usage: python3 tests/integration/membership.py BUILD_DIR
"""
import contextlib
import http.server
import json
import os
import socket
import sqlite3
import struct
import subprocess
import sys
import tempfile
import threading
import time
import uuid
from pathlib import Path


def receive(sock, size):
    data = b''
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        assert chunk, 'Server closed connection'
        data += chunk
    return data


def send(sock, kind, data, version=1):
    rid = str(uuid.uuid4())
    body = json.dumps(dict(protocol_version=version, request_id=rid, data=data)).encode()
    sock.sendall(struct.pack('!II', kind, len(body)) + body)
    return rid


def read(sock):
    while True:
        kind, length = struct.unpack('!II', receive(sock, 8))
        result = json.loads(receive(sock, length))
        if kind != 0x32:
            return result


def request(sock, kind, data, version=1):
    rid = send(sock, kind, data, version)
    result = read(sock)
    assert result['request_id'] == rid, result
    return result


def ok(result):
    assert result['success'], result
    return result['data'].get('result', result['data'])


class ModelFixture(http.server.BaseHTTPRequestHandler):
    calls = []
    mode = 'success'
    entered = threading.Event()
    release = threading.Event()

    def log_message(self, *_args):
        pass  # Never log headers or user questions.

    def do_POST(self):
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        type(self).calls.append((self.path, self.headers.get('Authorization'), body))
        mode = type(self).mode
        type(self).entered.set()
        if mode == 'wait':
            type(self).release.wait(5)
        if mode == 'timeout':
            type(self).release.wait(28)
        status = 429 if mode == 'limited' else 200
        payload = {'choices': [{'message': {'content': '请先预约空闲充电桩，再开始模拟充电。[1]'},
                                'finish_reason': 'stop'}]}
        if mode == 'invalid':
            payload = {'unexpected': True}
        if mode == 'truncated':
            payload['choices'][0]['finish_reason'] = 'length'
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        with contextlib.suppress(BrokenPipeError, ConnectionResetError):
            self.wfile.write(json.dumps(payload).encode())


@contextlib.contextmanager
def server(build, root, model_port, key=True):
    db_path = root / ('model.sqlite3' if key else 'no-key.sqlite3')
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    env = {k: v for k, v in os.environ.items() if not k.startswith('EV_AI_')}
    env.update(EV_AI_BASE_URL=f'http://127.0.0.1:{model_port}/v1', EV_AI_ALLOW_LOCAL_HTTP='1')
    if key:
        env['EV_AI_API_KEY'] = 'isolated-fixture-key'
    with open(root / f'{port}.log', 'w+') as log:
        process = subprocess.Popen([str(build / 'bin/ev_server'), '--host', '127.0.0.1',
                                    '--port', str(port), '--database', str(db_path)],
                                   env=env, stdout=log, stderr=log)
        try:
            for _ in range(150):
                try:
                    probe = socket.create_connection(('127.0.0.1', port), timeout=1)
                    probe.close()
                    break
                except OSError:
                    if process.poll() is not None:
                        log.seek(0)
                        raise AssertionError(log.read())
                    time.sleep(.05)
            else:
                raise AssertionError('Server startup timed out')
            yield port, db_path
        finally:
            process.terminate()
            process.wait(timeout=5)


def run(build):
    model = http.server.ThreadingHTTPServer(('127.0.0.1', 0), ModelFixture)
    threading.Thread(target=model.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory(prefix='ev-membership-integration-') as temporary:
            root = Path(temporary)
            with server(build, root, model.server_port) as (port, db_path), \
                    socket.create_connection(('127.0.0.1', port), timeout=35) as user, \
                    socket.create_connection(('127.0.0.1', port), timeout=35) as admin, \
                    sqlite3.connect(db_path) as db:
                login = ok(request(user, 1, dict(phone='19900000888', is_auto_register=True)))
                session = login['user_info']['session_id']
                uid = login['user_info']['user_id']
                admin_session = ok(request(admin, 1, dict(role='admin', username='admin', password='admin123')))['session_id']

                def feature(kind, action, **params):
                    return request(user, kind, dict(session_id=session, type=action, params=params))

                def management(kind, action, **params):
                    return request(admin, kind, dict(session_id=admin_session, type=action, params=params))

                def buy(plan, operation=None, version=1, consent=False):
                    return feature(0x40, 'purchase', plan_id=plan, version=version,
                                   operation_key=operation or str(uuid.uuid4()), renewal_consent=consent)

                payload = dict(session_id=session, type='status', params={})
                assert request(admin, 0x40, payload)['code'] == 'UNAUTHORIZED'
                assert request(user, 0x40, payload, version=99)['code'] == 'PROTOCOL_ERROR'
                assert management(0x40, 'status')['code'] == 'UNAUTHORIZED'
                assert request(user, 0x61, dict(payload, type='membership_update'))['code'] == 'UNAUTHORIZED'
                assert not ok(feature(0x40, 'status'))['valid']
                assert feature(0x50, 'ask', question='怎么充电')['code'] == 'FORBIDDEN'
                assert len(ok(feature(0x40, 'plans'))['plans']) == 12
                assert buy(7)['code'] == 'INSUFFICIENT_BALANCE'
                ok(feature(0x10, 'recharge', amount_cent=100000))
                operation = str(uuid.uuid4())
                assert buy(10, operation)['code'] == 'INVALID_INPUT'
                bought = ok(buy(10, operation, consent=True))
                assert bought['paid_cent'] == 2500
                repeated = ok(buy(10, operation, consent=True))
                assert repeated['replayed'] and repeated['expires_at'] == bought['expires_at']
                assert db.execute('SELECT balance FROM users WHERE user_id=?', (uid,)).fetchone()[0] == 975
                assert buy(1)['code'] == 'STATE_CONFLICT'
                ok(feature(0x40, 'set_renewal', enabled=False))
                state = ok(feature(0x40, 'status'))
                assert state['valid'] and state['renewal']['status'] == 'cancelled'
                assert db.execute('SELECT COUNT(*) FROM membership_operations WHERE user_id=?', (uid,)).fetchone()[0] == 1
                ok(management(0x61, 'membership_update', plan_id=10, version=1,
                              price_cent=2600, discount_bps=7900, active=True))
                assert ok(feature(0x40, 'status'))['discount_bps'] == 8000
                assert feature(0x40, 'set_renewal', enabled=True, plan_id=10, version=1, renewal_consent=True)['code'] == 'STATE_CONFLICT'
                ok(feature(0x40, 'set_renewal', enabled=True, plan_id=10, version=2, renewal_consent=True))
                ok(feature(0x40, 'set_renewal', enabled=False))
                assert management(0x61, 'membership_update', plan_id=10, version=2,
                                  price_cent=0, discount_bps=7900, active=True)['code'] == 'INVALID_INPUT'

                # A real discounted charge and order history retain the original terms.
                pile = db.execute("SELECT pile_id FROM charging_piles WHERE status='idle' LIMIT 1").fetchone()[0]
                reservation = ok(feature(0x30, 'reserve', pile_id=pile))
                oid = reservation['order_id']
                assert reservation['discount_bps'] == 8000
                ok(feature(0x30, 'start_charge', order_id=oid))
                db.execute("UPDATE orders SET start_time=datetime('now','localtime','-3 minutes') WHERE order_id=?", (oid,))
                db.commit()
                settlement = ok(feature(0x30, 'end_charge', order_id=oid))
                assert settlement['settled'] and settlement['gross_fee_cent'] > settlement['total_fee_cent'] > 0
                order = next(o for o in ok(feature(0x10, 'query_orders'))['orders'] if o['order_id'] == oid)
                assert order['discount_bps'] == 8000 and order['gross_fee_cent'] == settlement['gross_fee_cent']
                assert order['discount_fee_cent'] == settlement['gross_fee_cent'] - settlement['total_fee_cent']

                unknown = ok(feature(0x50, 'ask', question='写一首春天的诗'))
                assert not unknown['generated'] and not unknown['sources'] and not ModelFixture.calls
                answer = ok(feature(0x50, 'ask', question='怎么预约充电'))
                assert answer['generated'] and answer['sources'][0]['version'] == 1
                assert all('content' not in item for item in answer['sources'])
                path, header, body = ModelFixture.calls[-1]
                assert path == '/v1/chat/completions' and header == 'Bearer isolated-fixture-key'
                assert body['model'] == 'glm-4.7-flash' and body['thinking']['type'] == 'disabled'
                wire = json.dumps(body, ensure_ascii=False)
                assert session not in wire and '19900000888' not in wire and 'isolated-fixture-key' not in wire
                assert feature(0x50, 'ask', question='怎么充电')['code'] == 'BUSY'

                # Reuse session after each cooldown to prove every question is authorized.
                for mode in ('limited', 'invalid', 'truncated'):
                    time.sleep(3.1)
                    ModelFixture.mode = mode
                    error = feature(0x50, 'ask', question='怎么充电')
                    assert error['code'] == 'MODEL_UNAVAILABLE', error
                    assert not error['data']['result'].get('answer')
                time.sleep(3.1)
                ModelFixture.mode = 'wait'
                ModelFixture.entered.clear(); ModelFixture.release.clear()
                rid = send(user, 0x50, dict(session_id=session, type='ask', params={'question': '怎么充电'}))
                assert ModelFixture.entered.wait(2)
                clear_id = send(user, 0x50, dict(session_id=session, type='clear', params={}))
                cleared = read(user); assert cleared['request_id'] == clear_id and cleared['success']
                ModelFixture.release.set()
                discarded = read(user); assert discarded['request_id'] == rid and discarded['code'] == 'STATE_CONFLICT'
                time.sleep(3.1)
                ModelFixture.mode = 'wait'; ModelFixture.entered.clear(); ModelFixture.release.clear()
                rid = send(user, 0x50, dict(session_id=session, type='ask', params={'question': '怎么充电'}))
                assert ModelFixture.entered.wait(2)
                assert len(ModelFixture.calls[-1][2]['messages']) == 2, 'Cleared history leaked into next question'
                article = ok(management(0x60, 'knowledge_list'))['articles'][-1]
                ok(management(0x61, 'knowledge_disable', article_id=article['article_id'], draft_version=article['draft_version']))
                ModelFixture.release.set()
                discarded = read(user); assert discarded['request_id'] == rid and discarded['code'] == 'STATE_CONFLICT'
                ok(management(0x61, 'knowledge_publish', article_id=article['article_id'], draft_version=article['draft_version']))
                time.sleep(3.1)
                ModelFixture.mode = 'timeout'; ModelFixture.release.clear()
                assert feature(0x50, 'ask', question='怎么充电')['code'] == 'MODEL_UNAVAILABLE'
                ModelFixture.release.set()

                db.execute("UPDATE membership_entitlements SET starts_at=strftime('%s','now')-2,expires_at=strftime('%s','now')-1 WHERE user_id=?", (uid,))
                db.commit()
                calls = len(ModelFixture.calls)
                assert feature(0x50, 'ask', question='怎么充电')['code'] == 'FORBIDDEN'
                ok(buy(1))  # VIP still may not use AI.
                assert feature(0x50, 'ask', question='怎么充电')['code'] == 'FORBIDDEN'
                db.execute("UPDATE users SET status='frozen' WHERE user_id=?", (uid,)); db.commit()
                assert feature(0x40, 'status')['code'] == 'ACCOUNT_FROZEN'
                assert feature(0x50, 'ask', question='怎么充电')['code'] in ('ACCOUNT_FROZEN', 'UNAUTHORIZED')
                assert len(ModelFixture.calls) == calls
                print('Membership TCP integration passed: authenticated purchase, idempotency, pricing, cancellation, discount/order history, knowledge versions, AI source/response handling, rate limit, timeout, history clearing, expiry/frozen gates.')

            with server(build, root, model.server_port, key=False) as (port, _db), \
                    socket.create_connection(('127.0.0.1', port), timeout=5) as user:
                login = ok(request(user, 1, dict(phone='19900000887', is_auto_register=True)))
                session = login['user_info']['session_id']
                ok(request(user, 0x10, dict(session_id=session, type='recharge', params={'amount_cent': 10000})))
                ok(request(user, 0x40, dict(session_id=session, type='purchase', params=dict(plan_id=7, version=1, operation_key=str(uuid.uuid4())))))
                result = request(user, 0x50, dict(session_id=session, type='ask', params={'question': '怎么充电'}))
                assert result['code'] == 'MODEL_UNAVAILABLE' and 'EV_AI_API_KEY' in result['message']
                print('No-key integration passed: SVIP receives configuration message, never a fabricated AI answer.')
    finally:
        ModelFixture.release.set()
        model.shutdown(); model.server_close()


if __name__ == '__main__':
    run(Path(sys.argv[1]).resolve())
