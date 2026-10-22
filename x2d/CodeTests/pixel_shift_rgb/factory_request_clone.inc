/* 4.2.0 原厂三个参数映射重建。仅支持已实测的类型；未知类型拒绝。
 * 元数据按字节复制，不输出、持久化或修改其内容。 */
extern const char *dbus_message_get_path(void *);
extern const char *dbus_message_get_interface(void *);
extern const char *dbus_message_get_member(void *);
extern void dbus_message_iter_init_append(void *,Iter *);
extern int dbus_message_iter_open_container(Iter *,int,const char *,Iter *);
extern int dbus_message_iter_close_container(Iter *,Iter *);
extern int dbus_message_iter_append_basic(Iter *,int,const void *);
extern int dbus_message_iter_append_fixed_array(Iter *,int,const void *,int);
static int equal_values(Iter *a,Iter *b,unsigned depth){
 if(depth>5)return 0;
 do{
  int t=dbus_message_iter_get_arg_type(a);
  if(t!=dbus_message_iter_get_arg_type(b))return 0;
  if(!t)return 1;
  if(t=='a'||t=='e'||t=='v'){
   Iter x={0},y={0};dbus_message_iter_recurse(a,&x);dbus_message_iter_recurse(b,&y);
   if(!equal_values(&x,&y,depth+1))return 0;
  }else if(t=='s'){
   const char *x=0,*y=0;dbus_message_iter_get_basic(a,&x);dbus_message_iter_get_basic(b,&y);
   if(!x||!y||!same(x,y))return 0;
  }else if(t=='i'||t=='u'||t=='t'||t=='y'){
   uint64_t x=0,y=0;dbus_message_iter_get_basic(a,&x);dbus_message_iter_get_basic(b,&y);
   if(x!=y)return 0;
  }else return 0;
  int more=dbus_message_iter_next(a);if(more!=dbus_message_iter_next(b))return 0;
  if(!more)return 1;
 }while(1);
}
static int clone_value(Iter *src,Iter *dst) {
 int t=dbus_message_iter_get_arg_type(src);
 if(t=='i'||t=='u'){uint32_t v=0;dbus_message_iter_get_basic(src,&v);return dbus_message_iter_append_basic(dst,t,&v);}
 if(t=='t'){uint64_t v=0;dbus_message_iter_get_basic(src,&v);return dbus_message_iter_append_basic(dst,t,&v);}
 if(t=='s'){const char *v=0;dbus_message_iter_get_basic(src,&v);return v&&dbus_message_iter_append_basic(dst,t,&v);}
 if(t=='a'){
  Iter values={0},array={0};void *bytes=0;int n=0;
  dbus_message_iter_recurse(src,&values);
  if(dbus_message_iter_get_arg_type(&values)!='y')return 0;
  dbus_message_iter_get_fixed_array(&values,&bytes,&n);
  if(n<=0||n>16777216||!bytes)return 0;
  if(!dbus_message_iter_open_container(dst,'a',"y",&array))return 0;
  if(!dbus_message_iter_append_fixed_array(&array,'y',&bytes,n))return 0;
  return dbus_message_iter_close_container(dst,&array);
 }
 return 0;
}
#include "factory_descriptor_rewrite.h"
/* geometry非空仅重建消息；调用方仍须处理缓冲寿命、拓扑及元数据。
 * 不提供直接部署开关；现有一亿入口继续传入空geometry。 */
static void *rebuild_request_sized(void *event,const uint64_t *replacement,
 const FactoryBufferGeometry *geometry,const uint64_t *allocated){
 FactoryDescriptorRewrite rewrite[2];
 if(geometry){
  if(!same(dbus_message_get_path(event),"/camera")||
     !same(dbus_message_get_interface(event),"com.hasselblad.camera")||
     !same(dbus_message_get_member(event),"reprocess_exposure_item"))return 0;
  if(!replacement||!allocated)return 0;
  for(unsigned side=0;side<2;side++)
   if(!factory_descriptor_begin(&rewrite[side],side,&geometry[side],allocated[side],replacement[side]))return 0;
 }
 void *m=dbus_message_new_method_call("com.hasselblad.camera",dbus_message_get_path(event),dbus_message_get_interface(event),dbus_message_get_member(event));
 if(!m)return 0;
 Iter src={0},dst={0};unsigned index=0,changed=0;
 if(!dbus_message_iter_init(event,&src))goto fail;
 dbus_message_iter_init_append(m,&dst);
 do{
  if(index>=3||dbus_message_iter_get_arg_type(&src)!='a')goto fail;
  Iter from={0},to={0};unsigned count=0;
  dbus_message_iter_recurse(&src,&from);
  if(!dbus_message_iter_open_container(&dst,'a',"{sv}",&to))goto fail;
  while(dbus_message_iter_get_arg_type(&from)){
   Iter pair={0},value={0},entry={0},variant={0};const char *key=0;
   if(++count>64||dbus_message_iter_get_arg_type(&from)!='e')goto fail;
   dbus_message_iter_recurse(&from,&pair);
   if(dbus_message_iter_get_arg_type(&pair)!='s')goto fail;
   dbus_message_iter_get_basic(&pair,&key);
   if(!key||!dbus_message_iter_next(&pair)||dbus_message_iter_get_arg_type(&pair)!='v')goto fail;
   dbus_message_iter_recurse(&pair,&value);int type=dbus_message_iter_get_arg_type(&value);
   char sig[3]={(char)type,0,0};if(type=='a'){sig[1]='y';}
   if(index==1){if(!same(key,"DCAM_CONTAINER")||type!='a'||count!=1)goto fail;}
   else if(type!='i'&&type!='u'&&type!='t'&&type!='s')goto fail;
   if(!dbus_message_iter_open_container(&to,'e',0,&entry)||!dbus_message_iter_append_basic(&entry,'s',&key))goto fail;
   if(!dbus_message_iter_open_container(&entry,'v',sig,&variant))goto fail;
   if(geometry&&index!=1){
    uint64_t original=0,updated=0;unsigned side=index==0?0:1;
    if(type=='i'||type=='u'||type=='t')dbus_message_iter_get_basic(&value,&original);
    int action=factory_descriptor_field(&rewrite[side],key,type,original,&updated);
    if(!action)goto fail;
    if(action==1){if(!clone_value(&value,&variant))goto fail;}
    else if(type=='t'){if(!dbus_message_iter_append_basic(&variant,type,&updated))goto fail;}
    else {uint32_t v=(uint32_t)updated;if(!dbus_message_iter_append_basic(&variant,type,&v))goto fail;}
   }else if(replacement&&index!=1&&same(key,"SHARE_HANDLE")){
    unsigned side=index==0?0:1;
    if(type!='t'||!replacement[side]||!dbus_message_iter_append_basic(&variant,'t',&replacement[side]))goto fail;
    if(changed&(1u<<side))goto fail;changed|=1u<<side;
   }else if(!clone_value(&value,&variant))goto fail;
   if(dbus_message_iter_next(&value))goto fail;
   if(!dbus_message_iter_close_container(&entry,&variant)||!dbus_message_iter_close_container(&to,&entry))goto fail;
   if(dbus_message_iter_next(&pair))goto fail;
   dbus_message_iter_next(&from);
  }
  if(!count||!dbus_message_iter_close_container(&dst,&to))goto fail;
  index++;
 }while(dbus_message_iter_next(&src));
 if(index!=3)goto fail;
 if(geometry){
  if(!factory_descriptor_complete(&rewrite[0])||!factory_descriptor_complete(&rewrite[1]))goto fail;
  return m;
 }
 if(replacement){if(changed!=3)goto fail;return m;}
 /* 原尺寸基线必须逐项完全相等，元数据逐字节相等；不输出值。 */
 Iter check_a={0},check_b={0};
 if(!dbus_message_iter_init(event,&check_a)||!dbus_message_iter_init(m,&check_b)||!equal_values(&check_a,&check_b,0))goto fail;
 text("REBUILT_ARGUMENTS_IDENTICAL\n");
 return m;
fail:
 dbus_message_unref(m);return 0;
}
static void *rebuild_request(void *event,const uint64_t *replacement){
 return rebuild_request_sized(event,replacement,0,0);
}
