#include "focus_model.hpp"
#include <limits>
namespace x2dui {
void FocusModel::observe(const FocusSnapshot&s,uint64_t now){
 if(closed_)return;bool changed=s.mode!=current.mode||s.valid!=current.valid;current=s;
 if(changed&&s.valid)selected_=s.mode>=1&&s.mode<=3?s.mode-1:-1;
 if(pending_>=0){if(now>deadline_){pending_=-1;failed_=true;}else if(s.valid&&s.mode==pending_){pending_=-1;closed_=true;}else if(now==deadline_){pending_=-1;failed_=true;}}
}
FocusAction FocusModel::choose(int i,uint64_t now,int&mode){
 if(!allowed(i)||now>std::numeric_limits<uint64_t>::max()-3000)return FocusAction::None;
 selected_=i;failed_=false;mode=i+1;if(current.mode==mode){closed_=true;return FocusAction::Close;}
 pending_=mode;deadline_=now+3000;return FocusAction::SetMode;
}
void FocusModel::move(int step){if(closed_||pending_>=0||(step!=1&&step!=-1))return;int start=selected_<0?(step>0?-1:3):selected_;for(int i=start+step;i>=0&&i<3;i+=step)if(allowed(i)){selected_=i;return;}}
const char16_t*focus_text(int language,int field){
 static const char16_t*const table[11][5]={
 {u"Single autofocus",u"Continuous autofocus",u"Manual focus",u"Applying…",u"Setting not applied. Please retry."},
 {u"Enfoque único",u"Enfoque continuo",u"Enfoque manual",u"Aplicando…",u"No se ha aplicado. Reintente."},
 {u"Mise au point unique",u"Mise au point continue",u"Mise au point manuelle",u"Application…",u"Échec du réglage. Réessayez."},
 {u"Einzelautofokus",u"Kontinuierlicher Autofokus",u"Manueller Fokus",u"Wird eingestellt…",u"Nicht übernommen. Erneut versuchen."},
 {u"Fuoco singolo",u"Fuoco continuo",u"Fuoco manuale",u"Applicazione…",u"Impostazione non applicata. Riprovare."},
 {u"Enkel autofokus",u"Kontinuerlig autofokus",u"Manuell fokus",u"Tillämpar…",u"Inställningen misslyckades. Försök igen."},
 {u"Покадровый фокус",u"Непрерывный фокус",u"Ручной фокус",u"Применение…",u"Не применено. Повторите попытку."},
 {u"シングルフォーカス",u"コンティニュアスフォーカス",u"マニュアルフォーカス",u"設定中…",u"設定できませんでした。再試行してください。"},
 {u"单次对焦",u"连续对焦",u"手动对焦",u"正在设置…",u"设置未完成，请重试"},
 {u"單次對焦",u"連續對焦",u"手動對焦",u"正在設定…",u"設定未完成，請重試"},
 {u"단일 자동 초점",u"연속 자동 초점",u"수동 초점",u"설정 중…",u"설정되지 않았습니다. 다시 시도하세요."}};
 if(field<0||field>=5)return u"";return table[language>=0&&language<11?language:0][field];
}
}
