"""固定旧 p69 图像 oracle 与本机字体，仅内部离屏测试使用。"""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[4]
HERE=Path(__file__).resolve().parent
R=ROOT/'x2d/outputs/4.2.0/installer-hardening-r7'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def prepare():
 assert Path.cwd().resolve()==ROOT
 base=R/'native-payload/revisions/daf41916a46d4978cd8d/private-payload/install'
 sources=[base/n for n in ('p69-X2dShutterAnimation.qml','p70-X2dShutterAnimationHost.qml')]
 fonts=[Path('C:/Windows/Fonts')/n for n in ('segoeui.ttf','seguisym.ttf','msyh.ttc')]
 identity={p.name:sha(p) for p in sources+fonts};identity['builder']=sha(Path(__file__))
 rev=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out=R/'native-ui/native-animation-oracle'/rev
 if out.exists():
  meta=json.loads((out/'report.json').read_text(encoding='utf8'));assert meta['identity']==identity
  assert all(sha(out/p.name)==identity[p.name] for p in sources)
  assert all(sha(out/'private-fonts'/p.name)==identity[p.name] for p in fonts)
  assert sha(out/'oracle.qml')==meta['adapted_sha256'];return out
 out.mkdir(parents=True,exist_ok=False);(out/'private-fonts').mkdir()
 for p in sources:(out/p.name).write_bytes(p.read_bytes())
 for p in fonts:(out/'private-fonts'/p.name).write_bytes(p.read_bytes())
 text=sources[0].read_text(encoding='utf8');marker='function soundRequest(action)';start=text.index('{',text.index(marker));depth=1;i=start+1
 while depth:
  if text[i]=='{':depth+=1
  elif text[i]=='}':depth-=1
  i+=1
 text=text[:start+1]+' return; '+text[i-1:];assert text.count('property bool networkEnabled: true')==1;text=text.replace('property bool networkEnabled: true','property bool networkEnabled: false');assert 'XMLHttpRequest' not in text
 (out/'oracle.qml').write_text(text,encoding='utf8')
 (out/'X2dEffectPreferences.qml').write_text('import QtQml\nQtObject { property bool networkEnabled: false; property bool ready: true; property int mode: 1 }\n',encoding='utf8')
 (out/'report.json').write_text(json.dumps(dict(identity=identity,adapted_sha256=sha(out/'oracle.qml'),scope='Private old p69 visual oracle. Sound request body disabled and preferences explicit inert fixture. No production source edited.',sizes=[[800,600],[400,300],[900,600],[600,800]],progress=[0,.125,.2125,.25,.4,.5,.625,.75,.9,1],thresholds=dict(mean_max_channel_error=3.0,fraction_pixels_error_gt16=.025),fonts_private_only=True,camera_operations=0),ensure_ascii=False,indent=2),encoding='utf8');return out
if __name__=='__main__':print(prepare())
