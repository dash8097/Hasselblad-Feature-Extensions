"""不导入USB。验证新UI核验替换范围、单次请求与失败保留。"""
import ast,hashlib,json,types
from pathlib import Path
import build_native_client as b
def main():
 assert Path.cwd().resolve()==b.ROOT
 original=ast.parse(b.transformed_patch(False)[0]);native=ast.parse(b.transformed_patch(True)[0])
 def methods(tree):return {n.name:n for n in next(x for x in tree.body if isinstance(x,ast.ClassDef) and x.name=='Installer').body if isinstance(n,ast.FunctionDef)}
 old,new=methods(original),methods(native);assert old.keys()==new.keys()
 unchanged=[]
 for name in old:
  if name!='verify_current':assert ast.dump(old[name])==ast.dump(new[name]);unchanged.append(name)
 value=ast.unparse(new['verify_current']);assert value.count('native_client_support.verify_ui(self)')==1
 for forbidden in ('MENU_AND_AFC_POINTERS_READY','rawStackUI=3 popup=1','x2d-native-menu-preload.status'):assert forbidden not in value
 assert "obj.get('eyeProtocol') == 2" in value
 diagnostic=b.native_ui_diagnostic_command()
 assert 'head -c 512 $f' in diagnostic and '/tmp/x2d-native-ui.status' in diagnostic
 disable=b.native_ui_disable_diagnostic_command()
 assert all(v in disable for v in ('DISABLE_MARKER=','echo UNCONFIRMED','echo PRESENT','echo ABSENT','d=/blackbox','[ -L $d ]','[ ! -r $d ]','[ ! -x $d ]'))
 for command in (diagnostic,disable):
  assert len(command.encode('ascii'))<=231
  assert all(v not in command for v in ('x2d-native-ui.query','ui-ready','mkdir','rm ','reboot','kill','mount ','>','dbus-send'))
 source=ast.parse((b.HERE/'native_client_support.py').read_text(encoding='utf8'));fn=next(n for n in source.body if isinstance(n,ast.FunctionDef) and n.name=='verify_ui')
 def check(ok,label):
  if not ok:raise ValueError(label)
 scope={'check':check};exec(compile(ast.Module(body=[fn],type_ignores=[]),'<actual verify_ui>','exec'),scope)
 class Fake:
  def __init__(self,reply='NATIVE_UI_READY_CURRENT_REQUEST',error=False,caps=127):self.native_metadata={'native_ui':{'schema':'X2D_NATIVE_UI_CONTRACT_V1','required_capabilities':caps}};self.calls=[];self.log=[];self.reply=reply;self.error=error
  def one(self,command):
   self.calls.append(command)
   if self.error:raise RuntimeError('transport failure')
   return self.reply
 rows=[]
 for reply in ('NATIVE_UI_READY_CURRENT_REQUEST','MENU_AND_AFC_POINTERS_READY','ABSENT','NATIVE_UI_READY_CURRENT_REQUEST\nNATIVE_UI_READY_CURRENT_REQUEST',''):
  f=Fake(reply);accepted=False
  try:scope['verify_ui'](f);accepted=True
  except ValueError:pass
  assert accepted==(reply=='NATIVE_UI_READY_CURRENT_REQUEST') and f.calls==['/system/bin/x2d-native-flow ui-ready'];assert bool(f.log)==accepted
  rows.append({'reply':reply,'accepted':accepted,'requests':len(f.calls)})
 for caps in (0,-1,True,2**64,None):
  f=Fake(caps=caps)
  try:scope['verify_ui'](f);raise AssertionError('invalid caps accepted')
  except ValueError:pass
  assert not f.calls
 f=Fake(error=True)
 try:scope['verify_ui'](f);raise AssertionError('transport failure hidden')
 except RuntimeError as e:assert str(e)=='transport failure'
 assert len(f.calls)==1 and not f.log
 identity={n:hashlib.sha256((b.HERE/n).read_bytes()).hexdigest() for n in ('build_native_client.py','native_client_support.py','test_native_ui_client_contract.py')}
 report=dict(identity=identity,passed=True,unchanged_installer_methods=unchanged,cases=rows,invalid_contract_cases=5,transport_error_preserved=True,diagnostic_fixed_passive_command_checked=True,hardware_operations=0,actual_client_executed=False)
 out=b.R7/'native-host/ui-contract-tests'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out.mkdir(parents=True,exist_ok=False);(out/'report.json').write_bytes(b.encoded(report));print(out)
if __name__=='__main__':main()
