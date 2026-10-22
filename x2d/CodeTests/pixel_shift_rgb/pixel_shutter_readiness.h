#ifndef PIXEL_SHUTTER_READINESS_H
#define PIXEL_SHUTTER_READINESS_H
/* 固定 worker 协议 3：必须是当前参数的准备结果，不能借用旧模式的 ready。 */
static int po_shutter_prepared(const char *status,const char *arm){
 const char *flag=strstr(status,",\"prepared\":true,");
 const char *field=strstr(status,",\"prepared_arm\":\"");
 if(!flag||!field)return 0;
 field+=strlen(",\"prepared_arm\":\"");
 size_t length=strcspn(arm,"\n");
 return length>0&&length<128&&strlen(field)>length&&!memcmp(field,arm,length)&&field[length]=='"';
}
#endif
