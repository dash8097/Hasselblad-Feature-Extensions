#include "multishot_settings_view.hpp"
#include <new>
#include <cmath>
#include <cstdio>
namespace x2dui {
static bool finitePositive(double v){return std::isfinite(v)&&v>0&&v<=8192;}
static void text_size(AbiViewProfile*a,void*o,double size){alignas(8)unsigned char font[16];a->font_ctor(font);int status=-1,flags=0;void*args[]={font,nullptr,&status,&flags};a->text(o,1,1,args);a->font_size(font,int(size));a->text(o,2,1,args);a->font_dtor(font);}
bool MultishotSettingsView::bind(void*o,Event e,int i){auto*b=new(std::nothrow)Binding(this,i);if(!b)return false;retain();if(api.listen(api.context,o,e,event,b,released))return true;released(b);return false;}
void MultishotSettingsView::released(void*p){auto*b=static_cast<Binding*>(p);auto*s=b->self;delete b;s->drop();}
void MultishotSettingsView::releaseBack(void*p){static_cast<MultishotSettingsView*>(p)->drop();}
void MultishotSettingsView::back(void*p){static_cast<MultishotSettingsView*>(p)->key(SettingsKey::Back);}
void MultishotSettingsView::hidePopup(){model.close_popup();dragging=false;if(popup)api.visible(api.context,popup,false);}
void MultishotSettingsView::event(void*p,Event e,double y){
 auto*b=static_cast<Binding*>(p);auto&s=*b->self;
 if(e==Event::Destroy){s.root=nullptr;s.alive=false;return;}if(!s.alive||s.closed)return;
 if(e==Event::Hidden){s.hidePopup();return;}
 if(b->index==-2&&e==Event::Click){if(s.interactive)s.accept();return;}
 if(!s.interactive||!std::isfinite(y))return;
 if(e==Event::Press){s.dragging=true;s.moved=false;s.pressY=y;s.pressOffset=s.offset;s.pressIndex=s.selected;return;}
 if(e==Event::Move&&s.dragging){double delta=s.pressY-y;if(std::abs(delta)>8)s.moved=true;
  if(s.model.popup){double cell=(s.height-s.style.selectorTop-s.style.selectorBottom)/s.style.visibleChoices;int shift=int(delta/cell+(delta>=0?.5:-.5));s.selected=s.pressIndex+shift;if(s.selected<0)s.selected=0;if(s.selected>=s.options.count)s.selected=s.options.count-1;}
  else s.offset=s.pressOffset+delta;s.arrange();return;}
 if(e==Event::Release){bool down=s.dragging;s.dragging=false;if(down&&!s.moved){if(b->index>=100){int i=s.selected+b->index-103;if(i>=0&&i<s.options.count){s.selected=i;s.accept();}}else if(b->index>=0&&b->index<10)s.openRow(b->index);}}
}
void MultishotSettingsView::openRow(int row){if(!model.open_row(row))return;if(model.popup){options=model.choices();if(!options.valid){hidePopup();return;}selected=options.index<0?0:options.index;api.visible(api.context,popup,true);}else offset=0;redraw();}
void MultishotSettingsView::accept(){if(!alive||!model.popup||!model.editable()||selected<0||selected>=options.count)return;
 // Preserve current state if the owning controller rejects the request.
 auto old=model.data;auto change=model.choose(options.option,options.values[selected]);if(change.option==SettingsOption::None)return;
 retain();bool ok=changed&&changed(context,change);if(alive&&!closed){if(!ok)model.data=static_cast<SettingsSnapshot&&>(old);else hidePopup();redraw();}drop();
}
void MultishotSettingsView::key(SettingsKey k){if(!alive||closed)return;
 if(k==SettingsKey::Back&&model.popup){hidePopup();return;}
 if(k==SettingsKey::External){retain();if(model.popup)accept();if(alive&&!closed&&left)left(context,SettingsExit::External);drop();return;}
 if(k==SettingsKey::Back){auto result=model.leave(false);if(result!=SettingsExit::None){retain();if(left)left(context,result);drop();return;}offset=0;redraw();return;}
 if(k==SettingsKey::Accept){if(model.popup)accept();else openRow(model.currentRow);return;}
 int direction=k==SettingsKey::Up?-1:1;if(model.popup){selected+=direction;if(selected<0)selected=0;if(selected>=options.count)selected=options.count-1;arrange();}
 else {model.move(direction);double top=model.currentRow*style.rowHeight,bottom=top+style.rowHeight;if(top<offset)offset=top;if(bottom>offset+height-style.headerHeight)offset=bottom-height+style.headerHeight;arrange();}
}
void MultishotSettingsView::arrange(){if(!alive||!root||!finitePositive(width)||!finitePositive(height))return;auto c=api.context;
 api.rect(c,root,0,0,width,height);api.rect(c,header,0,0,width,style.headerHeight);api.rect(c,title,width*.15,0,width*.7,style.headerHeight);
 double viewport=height-style.headerHeight,maxOffset=model.row_count()*style.rowHeight-viewport;if(maxOffset<0)maxOffset=0;if(offset<0)offset=0;if(offset>maxOffset)offset=maxOffset;
 api.rect(c,clip,0,style.headerHeight,width,viewport);api.rect(c,content,0,-offset,width,model.row_count()*style.rowHeight);
 for(int i=0;i<10;i++){double y=i*style.rowHeight;api.rect(c,rows[i],style.leftMargin,y,width-style.leftMargin-style.rightMargin,style.rowHeight);api.rect(c,touch[i],0,y,width,style.rowHeight);api.rect(c,rowHighlight[i],0,y,width,style.rowHeight);api.visible(c,rowHighlight[i],i<model.row_count()&&i==model.currentRow);}
 api.rect(c,popup,0,0,width,height);api.rect(c,shade,0,0,width,height);api.rect(c,outside,0,0,width,height);
 double pw=width-style.selectorLeft-style.selectorRight,ph=height-style.selectorTop-style.selectorBottom,cell=ph/style.visibleChoices;
 api.rect(c,frame,style.selectorLeft,style.selectorTop,pw,ph);api.rect(c,panel,2,2,pw-4,ph-4);api.rect(c,highlight,0,(ph-cell)/2-2,pw-4,cell);
 for(int i=0;i<7;i++){int index=selected+i-3;bool show=model.popup&&index>=0&&index<options.count;api.visible(c,labels[i],show);api.visible(c,buttons[i],show);api.rect(c,labels[i],2,(ph-cell)/2+(i-3)*cell,pw-8,cell);api.rect(c,buttons[i],2,(ph-cell)/2+(i-3)*cell,pw-8,cell);if(show)api.text(c,labels[i],options.labels[index].c_str(),cell*.6,1);}
}
bool MultishotSettingsView::redraw(){if(!alive||!root)return false;auto c=api.context;auto titleText=model.title();if(!titleText.valid())return false;api.text(c,title,titleText.c_str(),style.headerFont,1);
 bool ok=true;for(int i=0;i<10;i++){bool show=i<model.row_count();api.visible(c,rows[i],show);api.visible(c,touch[i],show);api.enabled(c,touch[i],show);if(!show)continue;auto label=model.label(i),value=model.value(i);if(!label.valid()||!value.valid())return false;
  ok=stock_set_text(stock,rows[i],"text",label.c_str())&&stock_set_text(stock,rows[i],"valueText",value.c_str())&&stock_set_number(stock,rows[i],"pixelSize",style.labelFont)&&ok;
  auto*right=stock_get_object(stock,rows[i],"rightText");if(!right)return false;text_size(abi,right,style.valueFont);api.enabled(c,rows[i],model.data.stack||model.editable());api.opacity(c,right,model.data.stack||model.row_editable(i)?1:.35);
 }
 arrange();return ok;
}
bool MultishotSettingsView::update(const SettingsSnapshot&data,const SettingsStyle&s,double w,double h,bool touchEnabled,bool external){
 if(!alive||closed)return false;
 if(!finitePositive(w)||!finitePositive(h)||!finitePositive(s.headerHeight)||s.headerHeight>=h||!finitePositive(s.rowHeight)||!finitePositive(s.labelFont)||!finitePositive(s.valueFont)||!finitePositive(s.headerFont)||!finitePositive(s.visibleChoices)||s.visibleChoices<1||s.visibleChoices>6||!std::isfinite(s.leftMargin)||!std::isfinite(s.rightMargin)||s.leftMargin<0||s.rightMargin<0||s.leftMargin+s.rightMargin>=w||!std::isfinite(s.selectorLeft)||!std::isfinite(s.selectorRight)||!std::isfinite(s.selectorTop)||!std::isfinite(s.selectorBottom)||s.selectorLeft<0||s.selectorRight<0||s.selectorTop<0||s.selectorBottom<0||s.selectorLeft+s.selectorRight>=w-4||s.selectorTop+s.selectorBottom>=h-4){api.visible(api.context,root,false);return false;}
 bool changedMode=model.data.stack!=data.stack||model.data.method!=data.method;model.data=data;if(!model.data.aperture.valid()||!model.data.shutter.valid()||!model.data.iso.valid()||!model.data.outputShutter.valid()){api.visible(api.context,root,false);return false;}
 style=s;width=w;height=h;interactive=touchEnabled;if(changedMode){hidePopup();model.editingTime=false;model.currentRow=0;offset=0;}if(!model.editable())hidePopup();
 if(model.popup){auto fresh=model.choices();if(!fresh.valid||fresh.option!=options.option)hidePopup();else {int oldValue=selected>=0&&selected<options.count?options.values[selected]:-1;options=static_cast<SettingsChoices&&>(fresh);selected=options.index<0?0:options.index;for(int i=0;i<options.count;i++)if(options.values[i]==oldValue)selected=i;}}
 auto c=api.context;api.color(c,root,s.background,false);api.color(c,title,s.foreground,true);api.color(c,frame,s.border,false);api.color(c,panel,s.background,false);api.color(c,highlight,s.highlight,false);for(auto*l:labels)api.color(c,l,s.foreground,true);for(auto*r:rowHighlight)api.color(c,r,s.highlight,false);
 stock_set_bool(stock,header,"externalMenu",external);api.enabled(c,header,interactive);bool ok=redraw();api.visible(c,root,ok);return ok;
}
MultishotSettingsView*MultishotSettingsView::create(ViewApi a,StockProfile s,SignalProfile g,AbiViewProfile*b,void*contextObject,void*parent,bool(*changed)(void*,const SettingsChange&),void(*left)(void*,SettingsExit),void*context){
 if(!b||!contextObject||!parent||!changed||!left||!a.create||!a.destroy||!a.rect||!a.text||!a.color||!a.visible||!a.enabled||!a.opacity||!a.name||!a.layer||!a.listen)return nullptr;
 auto*v=new(std::nothrow)MultishotSettingsView(a,s,g,b);if(!v)return nullptr;v->changed=changed;v->left=left;v->context=context;
 auto make=[&](Kind k,void*p){return a.create(a.context,k,p);};v->root=make(Kind::Rectangle,parent);if(!v->root){delete v;return nullptr;}
 if(!v->bind(v->root,Event::Destroy,-1)){a.destroy(a.context,v->root);delete v;return nullptr;}auto fail=[&](){v->close();return static_cast<MultishotSettingsView*>(nullptr);};
 a.name(a.context,v->root,"X2dMultishotSettings");a.layer(a.context,v->root,1500,false);v->header=stock_create(s,StockKind::MenuHeader,contextObject,v->root);v->title=make(Kind::Text,v->root);v->clip=make(Kind::Item,v->root);if(!v->header||!v->title||!v->clip)return fail();v->content=make(Kind::Item,v->clip);if(!v->content)return fail();a.layer(a.context,v->clip,0,true);
 v->retain();v->backWatch=signal_watch(g,v->header,v->root,"close()",back,v,releaseBack);if(!v->backWatch){v->drop();return fail();}
 for(int i=0;i<10;i++){v->rowHighlight[i]=make(Kind::Rectangle,v->content);v->rows[i]=stock_create(s,StockKind::TextValueRow,contextObject,v->content);v->touch[i]=make(Kind::Mouse,v->content);if(!v->rowHighlight[i]||!v->rows[i]||!v->touch[i])return fail();a.opacity(a.context,v->rowHighlight[i],.25);char name[64];std::snprintf(name,sizeof name,"X2dMultishotSettings_value%d",i);a.name(a.context,v->rows[i],name);std::snprintf(name,sizeof name,"X2dMultishotSettings_touch%d",i);a.name(a.context,v->touch[i],name);if(!v->bind(v->touch[i],Event::Press,i)||!v->bind(v->touch[i],Event::Move,i)||!v->bind(v->touch[i],Event::Release,i))return fail();}
 v->popup=make(Kind::Item,v->root);if(!v->popup)return fail();a.layer(a.context,v->popup,10,false);v->shade=make(Kind::Rectangle,v->popup);v->outside=make(Kind::Mouse,v->popup);v->frame=make(Kind::Rectangle,v->popup);if(!v->shade||!v->outside||!v->frame)return fail();a.color(a.context,v->shade,0,false);a.opacity(a.context,v->shade,.7);v->panel=make(Kind::Rectangle,v->frame);if(!v->panel)return fail();a.layer(a.context,v->panel,0,true);v->highlight=make(Kind::Rectangle,v->panel);if(!v->highlight||!v->bind(v->outside,Event::Click,-2)||!v->bind(v->root,Event::Hidden,-1))return fail();
 for(int i=0;i<7;i++){v->labels[i]=make(Kind::Text,v->panel);v->buttons[i]=make(Kind::Mouse,v->panel);if(!v->labels[i]||!v->buttons[i])return fail();char name[48];std::snprintf(name,sizeof name,"X2dSettingsChoice%d",i);a.name(a.context,v->buttons[i],name);if(!v->bind(v->buttons[i],Event::Press,100+i)||!v->bind(v->buttons[i],Event::Move,100+i)||!v->bind(v->buttons[i],Event::Release,100+i))return fail();}
 v->hidePopup();a.visible(a.context,v->root,false);return v;
}
void MultishotSettingsView::close(){if(closed)return;closed=true;signal_unwatch(backWatch);backWatch=nullptr;if(root){auto*p=root;root=nullptr;alive=false;api.destroy(api.context,p);}drop();}
}
