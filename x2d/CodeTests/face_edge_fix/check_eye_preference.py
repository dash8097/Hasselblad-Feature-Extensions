"""真实 JavaScript 引擎中的延迟选眼状态测试，不访问相机。"""
from pathlib import Path
import sys,os,json
D=Path(__file__).resolve().parent
O=D.parents[1]/'outputs/4.2.0/face-edge-fix'
sys.path.insert(0,str(O/'deps'))
os.environ['QT_QPA_PLATFORM']='offscreen'
from PySide6.QtCore import QCoreApplication
from PySide6.QtQml import QJSEngine
if os.environ.get('X2D_SIM_CASE_OUT'):
 O=Path(os.environ['X2D_SIM_CASE_OUT'])
app=QCoreApplication([]);e=QJSEngine()
v=e.evaluate((D/'EyePreferencePolicy.js').read_text('utf-8'))
assert not v.isError(),v.toString()
tests='''
var s=create(0,7),r;
function sample(l,r,t,p,id) {var v=step(s,p===undefined?0:p,id===undefined?7:id,l,r,t,500);s=v.state;return v}
function check(v,m){if(!v)throw new Error(m)}
var left={x:.3,y:.4,width:.0001,height:.0001},right={x:.6,y:.4,width:.0001,height:.0001};
check(nearestEye(left,right,305,300,1000,750)===0,"tap nearest left eye");
check(nearestEye(left,right,595,300,1000,750)===1,"tap nearest right eye");
check(nearestEye(null,right,305,300,1000,750)===1,"only available eye");
check(nearestEye(null,null,305,300,1000,750)===-1,"no invented eye");
r=sample(true,true,0);check(r.eye===0,"initial preference");
r=sample(false,true,100);check(r.eye===-1&&!r.fallbackFace,"no stale eye");
r=sample(true,true,400);check(r.eye===0,"brief blink preserves preference");
sample(false,true,500);
r=sample(false,true,999);check(r.eye===-1,"no premature switch");
r=sample(false,true,1000);check(r.eye===1,"switch after sustained loss");
r=sample(true,true,1100);check(r.eye===1,"no immediate bounce back");
r=sample(true,true,1600);check(r.eye===0,"stable preferred eye returns");
sample(false,false,1700);
r=sample(false,false,2200);check(r.eye===-1&&r.fallbackFace,"both missing fallback face");
r=sample(true,true,2300,1);check(r.eye===1,"user choice changes preference");
r=sample(true,true,2400,0,8);check(r.eye===0,"identity change resets state");
r=sample(true,true,2500,0,-1);check(r.eye===-1&&!r.fallbackFace,"no face cannot yield eye");
true;
'''
v=e.evaluate(tests)
assert not v.isError() and v.toBool(),v.toString()
(O/'eye-preference-policy-result.json').write_text(json.dumps(dict(passed=True,delayMs=500,deviceAccess=False,afIntegrated=False)),'utf-8')
print('EYE_PREFERENCE_POLICY_PASS; AF NOT INTEGRATED')
