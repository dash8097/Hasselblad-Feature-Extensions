#include "stock_factory.hpp"
namespace x2dui {
static AbiString text(const StockProfile&p,const char16_t*s){std::ptrdiff_t n=0;while(s[n])++n;return abi_owned_string(p,s,n);}
void*stock_create(const StockProfile&p,StockKind kind,void*contextObject,void*parentItem){
 static const char16_t*paths[]={u"qrc:/app/qml/exposescreen/ExposeScreen.qml",u"qrc:/app/qml/exposescreen/ExposeIconText.qml",u"qrc:/app/qml/exposescreen/ExposeProgress.qml",u"qrc:/app/qml/viewmodels/ExposeScreenViewModel.qml",u"qrc:/app/qml/components/TextValueRow.qml",u"qrc:/app/qml/mainmenu/MenuHeader.qml",u"qrc:/app/qml/components/Photo.qml",u"qrc:/app/qml/components/buttons/FramedImage.qml",u"qrc:/app/qml/mainmenu/SettingsGeneric.qml",u"qrc:/app/qml/components/TiledImage.qml",u"qrc:/app/qml/mainmenu/MenuBoolSelector.qml",u"qrc:/app/qml/popups/PopupIconText.qml",u"qrc:/app/qml/popups/Popover.qml",u"qrc:/app/qml/components/buttons/FramedItem.qml",u"qrc:/app/qml/components/Touchpad.qml"};
 unsigned k=static_cast<unsigned>(kind);if(k>=sizeof(paths)/sizeof(paths[0])||!contextObject||!parentItem||!p.cast(p.meta_item,parentItem))return nullptr;
 void*engine=p.engine(contextObject);if(!engine)return nullptr;void*context=p.context(engine);if(!context)return nullptr;
 void*component=p.allocate(p.component_size);if(!component)return nullptr;alignas(8)unsigned char url[8]{};auto path=text(p,paths[k]);p.url_construct(url,path,0);abi_release_string(p,path);
 p.component_construct(component,engine,url,0,nullptr);p.url_destroy(url);
 void*object=p.status(component)==1?p.create(component,context):nullptr;p.component_destroy(component);
 if(!object)return nullptr;if(k!=3&&!p.cast(p.meta_item,object)){p.object_destroy(object);return nullptr;}
 p.parent(object,parentItem);if(k!=3)p.visual_parent(object,parentItem);return object;
}
static bool has(const StockProfile&p,void*object,const char*property){
 if(!object||!property)return false;const void*table=*static_cast<const void**>(object);if(!table)return false;const void*fn=*static_cast<const void*const*>(table);
 if(p.valid_vtable&&!p.valid_vtable(p.validation_context,table,fn))return false;
 const void*meta=reinterpret_cast<const void*(*)(const void*)>(reinterpret_cast<std::uintptr_t>(fn))(object);return meta&&p.property_index(meta,property)>=0;
}
bool stock_has(const StockProfile&p,void*object,const char*name){return has(p,object,name);}
bool stock_get_size(const StockProfile&p,void*object,const char*name,int&w,int&h){if(!p.size_type||!has(p,object,name))return false;auto v=p.property(object,name);bool ok=p.variant_convert(&v,{p.size_type});if(ok){auto*data=static_cast<const int*>(p.variant_data(&v));if(!data)ok=false;else{w=data[0];h=data[1];}}p.variant_destroy(&v);return ok;}
bool stock_set_text(const StockProfile&p,void*object,const char*property,const char16_t*value){if(!value||!has(p,object,property))return false;alignas(8)unsigned char variant[32];auto string=text(p,value);p.variant_string(variant,string);abi_release_string(p,string);bool ok=p.set(object,property,variant);p.variant_destroy(variant);return ok;}
bool stock_set_bool(const StockProfile&p,void*object,const char*property,bool value){if(!has(p,object,property))return false;alignas(8)unsigned char variant[32];p.variant_bool(variant,value);bool ok=p.set(object,property,variant);p.variant_destroy(variant);return ok;}
bool stock_set_number(const StockProfile&p,void*object,const char*property,double value){if(!has(p,object,property))return false;alignas(8)unsigned char variant[32];p.variant_double(variant,value);bool ok=p.set(object,property,variant);p.variant_destroy(variant);return ok;}
bool stock_set_object(const StockProfile&p,void*object,const char*property,void*value){if(!has(p,object,property))return false;alignas(8)unsigned char variant[32];p.variant_typed(variant,{p.qobject_pointer_type},&value);bool ok=p.set(object,property,variant);p.variant_destroy(variant);return ok;}
void*stock_get_object(const StockProfile&p,void*object,const char*property){if(!has(p,object,property))return nullptr;auto variant=p.property(object,property);void*result=nullptr;if(p.variant_convert(&variant,{p.qobject_pointer_type})){void*data=p.variant_data(&variant);if(data)result=*static_cast<void**>(data);}p.variant_destroy(&variant);return result;}
bool stock_set_size(const StockProfile&p,void*object,const char*property,int w,int h){if(!p.size_type||!has(p,object,property)||w<0||h<0)return false;int size[2]={w,h};alignas(8)unsigned char v[32];p.variant_typed(v,{p.size_type},size);bool ok=p.set(object,property,v);p.variant_destroy(v);return ok;}
bool stock_override_number(const StockProfile&p,void*object,const char16_t*property,double value){if(!object||!property||!p.qml_property_construct||!p.qml_property_write||!p.qml_property_destroy)return false;alignas(8)unsigned char q[8],v[32];auto name=text(p,property);p.qml_property_construct(q,object,name);abi_release_string(p,name);p.variant_double(v,value);bool ok=p.qml_property_write(q,v);p.variant_destroy(v);p.qml_property_destroy(q);return ok;}
}
