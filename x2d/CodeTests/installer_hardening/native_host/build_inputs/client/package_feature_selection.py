"""已随归档校验的固定功能配置；不接触 USB，也不接受任意路径。"""
import copy

def select_features(manifest,mask):
    if type(mask) is not int or not 1<=mask<=255:
        raise ValueError('请选择至少一个扩展功能。')
    if manifest.get('feature_selection_version')!=1:
        raise ValueError('安装包缺少可选功能配置。')
    if mask not in manifest.get('allowed_feature_masks', []):
        raise ValueError('此测试包只提供完整功能配置。')
    profile=manifest['feature_profiles'][str(mask)]
    result=copy.deepcopy(manifest)
    for field in ('files','targets','original','file_sizes'):
        result[field]={n:manifest[field][n] for n in profile['names']}
    result['files'].update(profile['overrides'])
    result.update(feature_mask=mask,capture_included=bool(mask&7),face_eye_included=bool(mask&8),af_speed_included=bool(mask&16))
    return result

INSTALLED_MASK_COMMAND='f=/system/etc/x2d-feature-mask; if test ! -L "$f" && test -f "$f"; then head -c 4 "$f"; else echo LEGACY; fi'

def installed_mask(reply):
    reply=reply.strip()
    if reply=='LEGACY':return 255
    if not reply.isascii() or not reply.isdigit() or reply.startswith('0') or not 1<=int(reply)<=255:
        raise ValueError('已安装的功能记录无法读取；请查看诊断，不重复安装。')
    return int(reply)
