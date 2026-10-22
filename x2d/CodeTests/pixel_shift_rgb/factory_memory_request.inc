/* 4.2.0 已核对的 ImageMemory 描述及 DCAM_CONTAINER 参数类型。
 * 仅生成内存消息模板；虚拟句柄不会发送，后续必须替换为真实 ION 句柄。 */
static int memory_entry(Iter *map,const char *key,int type,const void *value,int bytes){
 Iter entry={0},variant={0},array={0};char signature[3]={(char)type,0,0};
 if(type=='a')signature[1]='y';
 if(!dbus_message_iter_open_container(map,'e',0,&entry)||
    !dbus_message_iter_append_basic(&entry,'s',&key)||
    !dbus_message_iter_open_container(&entry,'v',signature,&variant))return 0;
 if(type=='a'){
  if(bytes<=0||!value||!dbus_message_iter_open_container(&variant,'a',"y",&array)||
     !dbus_message_iter_append_fixed_array(&array,'y',&value,bytes)||
     !dbus_message_iter_close_container(&variant,&array))return 0;
 }else if(!dbus_message_iter_append_basic(&variant,type,value))return 0;
 return dbus_message_iter_close_container(&entry,&variant)&&dbus_message_iter_close_container(map,&entry);
}
static void *memory_request_template(const uint8_t *metadata,unsigned bytes){
 if(!metadata||!bytes||bytes>CBW_METADATA_MAX)return 0;
 void *message=dbus_message_new_method_call("com.hasselblad.camera","/camera",
  "com.hasselblad.camera","reprocess_exposure_item");
 if(!message)return 0;Iter args={0};dbus_message_iter_init_append(message,&args);
 for(unsigned index=0;index<3;index++){
  Iter map={0};if(!dbus_message_iter_open_container(&args,'a',"{sv}",&map))goto fail;
  if(index==1){if(!memory_entry(&map,"DCAM_CONTAINER",'a',metadata,(int)bytes))goto fail;}
  else{
   FactoryBufferGeometry geometry;unsigned side=index==0?0:1;
   if(!factory_buffer_geometry(&geometry,side?11656:11836,side?8742:8842,side?6:1))goto fail;
   uint64_t mask=factory_descriptor_mask(geometry.plane_count);
   for(unsigned field=0;field<22;field++)if(mask&(UINT64_C(1)<<field)){
    if(field==21){const char *label="pixel-shift-memory";
     if(!memory_entry(&map,factory_descriptor_keys[field],'s',&label,0))goto fail;
    }else if(field>=19){uint64_t number=field==19?1:0;
     if(!memory_entry(&map,factory_descriptor_keys[field],'t',&number,0))goto fail;
    }else{
     uint32_t number=factory_geometry_field(&geometry,field);
     if(!memory_entry(&map,factory_descriptor_keys[field],field==9||field==10?'u':'i',&number,0))goto fail;
    }
   }
  }
  if(!dbus_message_iter_close_container(&args,&map))goto fail;
 }
 return message;
fail:dbus_message_unref(message);return 0;
}
