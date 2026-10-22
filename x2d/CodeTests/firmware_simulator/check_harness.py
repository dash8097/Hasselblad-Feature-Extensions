"""核对本次输入/ELF/依赖摘要，并在独立guest中损坏参考数据证明会失败。"""
import json
import subprocess
from pathlib import Path
import run as sim


def payload(data, target):
    at = 0
    while at < len(data):
        if data[at:at+6] != b'070701':
            raise ValueError('Invalid newc header')
        fields = [int(data[at+6+8*i:at+14+8*i],16) for i in range(13)]
        size, namesize = fields[6], fields[11]
        name = data[at+110:at+110+namesize-1].decode()
        start = (at+110+namesize+3)&~3
        if name == target:
            return start, size
        at = (start+size+3)&~3
    raise ValueError('Guest fixture missing')


def main():
    if Path.cwd().resolve()!=sim.ROOT:
        raise ValueError('Wrong workspace')
    latest=json.loads((sim.OUT/'latest-attempt.json').read_text())
    folder=(sim.ROOT/latest['path']).resolve()
    if not folder.is_relative_to(sim.OUT) or not latest['passed']:
        raise ValueError('Latest run did not pass')
    report=json.loads((folder/'report.json').read_text(encoding='utf-8'))
    if not report['passed'] or report['all_features_passed']:
        raise ValueError('Unexpected qualification scope')
    for name,digest in report['source_sha256'].items():
        if sim.sha((sim.ROOT/name).read_bytes())!=digest:
            raise ValueError('Active source differs from tested revision: '+name)
    for name,entry in report['executables'].items():
        if sim.sha((folder/name).read_bytes())!=entry['sha256']:
            raise ValueError('ELF mismatch')
    for path,digest in report['firmware_files'].items():
        if sim.sha((folder/Path(path).name).read_bytes())!=digest:
            raise ValueError('Firmware dependency mismatch')
    image=bytearray((folder/'initramfs.cpio').read_bytes())
    if sim.sha(image)!=report['initramfs_sha256']:
        raise ValueError('Guest image mismatch')
    for name,info in report['fixtures'].items():
        start,size=payload(image,name)
        if size!=info['bytes'] or sim.sha(image[start:start+size])!=info['sha256']:
            raise ValueError('Reference fixture mismatch')
    start,_=payload(image,'fixtures/400-0.bin')
    image[start]^=1
    negative=folder/'negative-reference.cpio'; negative.write_bytes(image)
    args=list(report['command']); args[args.index('-initrd')+1]=str(negative)
    p=subprocess.run(args,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=60,
                     creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    output=p.stdout+p.stderr
    (folder/'negative-reference-console.txt').write_text(output,encoding='utf-8')
    rejected=(p.returncode==0 and 'X2D_SIM_ASSERT' in output and
              'name=400mp rc=1 timeout=0 expected=0' in output and
              'X2D_SIM_FAILED' in output and 'X2D_SIM_ALL_PASSED' not in output)
    check=dict(passed=rejected,positive_report_sha256=sim.sha((folder/'report.json').read_bytes()),
               checker_sha256=sim.sha(Path(__file__).read_bytes()),active_source_hashes_match=True,
               negative_initramfs_sha256=sim.sha(image),exactly_one_reference_bit_changed=True,
               negative_reference_rejected=rejected,independent_agent_review=False,real_camera=False)
    (folder/'harness-check.json').write_text(json.dumps(check,indent=2),encoding='utf-8')
    print('SIMULATOR_HASHES_AND_NEGATIVE_REFERENCE_PASS' if rejected else output)
    if not rejected:raise SystemExit(1)


if __name__=='__main__':main()
