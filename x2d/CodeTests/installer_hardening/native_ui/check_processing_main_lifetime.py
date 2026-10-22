"""独立复核借用 MainModel 的删除、替换和清空；真实 Qt，私有代理替身。"""
from pathlib import Path
import hashlib,json,os,subprocess,sys
import build_processing_tests as b
def main():
 assert Path.cwd().resolve()==b.ROOT
 original=b.OUT/'native-ui/pixel-runtime-service/f477debb05a7d99c0f62'
 report=json.loads((original/'report.json').read_text(encoding='utf8'))
 frozen=original/'private-source'
 for n,d in report['identity']['sources'].items():assert b.sha(frozen/n)==d
 test=(frozen/'test_pixel_runtime_service.cpp').read_text(encoding='utf8')
 marker='delete borrowed;check(!attachment.sync(window.contentItem(),true,s,800,600)'
 assert test.count(marker)==1
 extra='''
 auto*replacement=new QObject(vm);check(x2dui::stock_set_object(stock,vm,"main",replacement),"independent-main-replacement-written");
 check(attachment.sync(window.contentItem(),true,s,800,600),"independent-main-replacement-reopens");
 auto*replacementExposure=x2dui::stock_get_object(stock,attachment.item(),"viewModel");check(replacementExposure&&x2dui::stock_get_object(stock,replacementExposure,"main")==replacement,"independent-replacement-current-model");
 check(x2dui::stock_set_object(stock,vm,"main",nullptr),"independent-main-null-written");
 check(!attachment.sync(window.contentItem(),true,s,800,600)&&!attachment.item(),"independent-null-main-closes-live-page");
 check(x2dui::stock_set_object(stock,vm,"main",borrowed),"independent-restore-borrowed-main");
 check(attachment.sync(window.contentItem(),true,s,800,600),"independent-borrowed-reopens-after-null");
 delete replacement;
 '''
 test=test.replace(marker,extra+marker)
 identity=dict(author_report=b.sha(original/'report.json'),checker=b.sha(Path(__file__)),test=hashlib.sha256(test.encode()).hexdigest())
 out=b.OUT/'checker-ui/processing-main-lifetime'/hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:20];out.mkdir(parents=True,exist_ok=False)
 src=out/'private-source';src.mkdir()
 for n in report['identity']['sources']:(src/n).write_bytes((frozen/n).read_bytes())
 (src/'test_pixel_runtime_service.cpp').write_text(test,encoding='utf8')
 modules=['Quick','Qml','QmlModels','Gui','Network','Core','Test'];inc=['-I'+str(b.SDK/'include')]
 for m in modules:inc+=['-I'+str(b.SDK/'include'/('Qt'+m)),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'),'-I'+str(b.SDK/'include'/('Qt'+m)/'6.4.1'/('Qt'+m))]
 env=dict(os.environ,PATH=str(b.CXX.parent)+os.pathsep+str(b.SDK/'bin')+os.pathsep+os.environ['PATH'],TEMP=str(out),TMP=str(out),QML2_IMPORT_PATH=str(original/'private-fixture/imports')+os.pathsep+str(b.SDK/'qml'),QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QML_DISABLE_DISK_CACHE='1')
 cmd=[b.CXX,'-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-DQT_NO_DEBUG','-DQT_GUI_LIB','-DQT_CORE_LIB',*inc,*[src/n for n in report['identity']['sources'] if n.endswith('.cpp')],*[b.SDK/'lib'/('libQt6'+m+'.a') for m in modules],'-o',out/'test.exe']
 r=subprocess.run(list(map(str,cmd)),env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=120);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf8');assert not r.returncode,r.stdout+r.stderr
 r=subprocess.run([str(out/'test.exe'),str(original/'fixture.rcc')],env=env,capture_output=True,text=True,encoding='utf8',errors='replace',timeout=30);(out/'run.log').write_text(r.stdout+r.stderr,encoding='utf8')
 result=dict(identity=identity,passed=r.returncode==0,exit=r.returncode,stdout=r.stdout,stderr=r.stderr,independent_approval=r.returncode==0,added_cases=['borrowed-main-replacement','null-main-with-live-object','recover-original-after-null','delete-unreferenced-replacement'],camera_requests=0,actual_qt='desktop 6.4.1',hardware_tested=False)
 (out/'report.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8');print(out);print(r.stdout);assert not r.returncode,r.stdout+r.stderr
if __name__=='__main__':main()
