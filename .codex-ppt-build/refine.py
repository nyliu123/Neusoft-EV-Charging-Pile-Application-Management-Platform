import json
from pathlib import Path
p=Path('.codex-ppt-build/changes.json');d=json.loads(p.read_text(encoding='utf8'))
def add(n,a,b):d['changes'].setdefault(str(n),{})[a]=b
add(2,'难点解决与技能提升','难点解决、测试依据与技能提升')
add(7,'实现系统全部业务逻辑与数据处理，支持现代C++特性','使用现代 C++ 实现客户端、服务端与业务计算')
add(9,'主动退出 / 会话失效自动回登录页','退出或会话失效后返回登录页')
add(11,'调用浏览器打开路线规划','内嵌地图支持拖拽与缩放')
add(13,'异步请求，按钮禁用 + 「登录中...」状态','异步登录，等待期间禁用按钮')
add(14,'（需补充截图）','')
# Retain central screenshot placeholders, remove duplicate captions that overlap body/footer.
add(15,'（需补充截图）','');add(16,'（需补充截图）','')
m=d['newslides'][2]
for k,v in list(m.items()):
 if v=='用户登录、查站、选桩、预约、开始与结束充电。':m[k]='登录查站后，演示预约、开始充电和结束结算。'
 if v.startswith('Qt 6.2.4 与 GCC'):m[k]='Qt 6.2.4、GCC 11.3。测试数量引用会员模块验收文档，当前快照尚未重新运行。'
 if v.startswith('AI 使用本地模型夹具'):m[k]='本地模型夹具覆盖咨询成功和异常分支，真实供应商效果需单独验收。'
p.write_text(json.dumps(d,ensure_ascii=False),encoding='utf8')
