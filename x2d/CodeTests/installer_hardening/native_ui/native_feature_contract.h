#ifndef X2D_NATIVE_FEATURE_CONTRACT_H
#define X2D_NATIVE_FEATURE_CONTRACT_H
/* 两档严格解析。缺失或非法配置不启动任何补丁入口。 */
static inline unsigned x2d_native_feature_mask(const char*s){
 if(!s||s[0]!='2')return 0;
 if(s[1]=='4'&&s[2]=='8'&&s[3]==0)return 248u;
 if(s[1]=='5'&&s[2]=='5'&&s[3]==0)return 255u;
 return 0;
}
static inline int x2d_native_features_valid(const char*s){return x2d_native_feature_mask(s)!=0;}
static inline int x2d_native_features_advanced(unsigned mask){return mask==255u;}
static inline unsigned x2d_native_required_capabilities(unsigned mask){return mask==248u?4081u:mask==255u?4095u:0u;}
#define X2D_NATIVE_UI_FEATURE_ENV "X2D_NATIVE_UI_FEATURES"
#define X2D_NATIVE_UI_FEATURE_MASK 255u
#endif
