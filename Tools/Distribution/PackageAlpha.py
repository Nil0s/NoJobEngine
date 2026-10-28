"""Build a curated, relocatable Windows Alpha ZIP from a completed Release build."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,struct,zipfile
ROOT=Path(__file__).resolve().parents[2]
SCENES=['RLObstacleLearningDemo','RLTrackingDemo','RLNavigationDemo','NeuralTrainingDemo','NeuralPongTrackingDemo']
SCRIPTS=['RLObstacleAgent','RLTrackingAgent','RLTargetPatrol','PongAgent']
MODELS=['RLObstaclePolicy','RLTrackingPolicy','NeuralFollowerDemo','PongBrainDemo']
def cache_values(build):
    values={}
    for line in (build/'CMakeCache.txt').read_text(encoding='utf-8',errors='replace').splitlines():
        if line.startswith(('#','//')) or '=' not in line or ':' not in line.split('=',1)[0]:continue
        key,value=line.split('=',1);values[key.split(':',1)[0]]=value
    return values

def imports(path):
    """Read normal PE import names without launching a binary or relying on Visual Studio tools."""
    data=path.read_bytes();pe=struct.unpack_from('<I',data,0x3c)[0]
    if data[pe:pe+4]!=b'PE\0\0':raise ValueError(f'Not a PE image: {path.name}')
    count=struct.unpack_from('<H',data,pe+6)[0];optional_size=struct.unpack_from('<H',data,pe+20)[0];opt=pe+24
    directory=opt+(112 if struct.unpack_from('<H',data,opt)[0]==0x20b else 96)
    rva,size=struct.unpack_from('<II',data,directory+8)
    sections=[]
    for i in range(count):
        at=opt+optional_size+i*40
        vsize,vrva,rawsize,raw=struct.unpack_from('<IIII',data,at+8);sections.append((vrva,max(vsize,rawsize),raw))
    def offset(address):
        for start,length,raw in sections:
            if start<=address<start+length:return raw+address-start
        raise ValueError(f'Unmapped PE address in {path.name}')
    if not rva:return []
    result=[];at=offset(rva)
    while any(data[at:at+20]):
        name=offset(struct.unpack_from('<I',data,at+12)[0]);end=data.index(b'\0',name)
        result.append(data[name:end].decode('ascii').lower());at+=20
    return result

def package(build,output,crt):
    build=build.resolve();output=output.resolve();cache=cache_values(build)
    if cache.get('CMAKE_BUILD_TYPE') not in ('Release',None,''):raise ValueError('Package a Release build, never Debug.')
    if output.exists() and any(output.iterdir()):raise ValueError('Use a new empty output directory; existing contents are preserved.')
    archive=output.parent/(output.name+'.zip')
    if archive.exists():raise FileExistsError('Use a new package name; the ZIP already exists.')
    output.mkdir(parents=True,exist_ok=True)
    def copy(src,relative):
        if not src.is_file():raise FileNotFoundError(src)
        dest=output/relative;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
    def asset(relative):
        copy(ROOT/relative,relative)
        meta=ROOT/(relative+'.meta')
        if meta.exists():copy(meta,relative+'.meta')
    for target,public_name in [('NoJobEditor.exe','NJE.exe'),('NoJobRuntime.exe','NJERuntime.exe'),('NoJobTrainer.exe','NJETrainer.exe')]:
        candidates=[build/public_name,build/'Release'/public_name,build/target,build/'Release'/target]
        src=next((p for p in candidates if p.is_file()),None)
        if src is None:raise FileNotFoundError(public_name)
        copy(src,public_name)
    dlls=[p for p in (build/'ProjectScripts').rglob('NoJobProjectScripts_*.dll') if not p.stem.endswith('d')]
    if not dlls:raise FileNotFoundError('Release project-script DLL is missing.')
    copy(max(dlls,key=lambda p:p.stat().st_mtime),'RuntimeData/ProjectScripts/NoJobProjectScripts_alpha1.dll')
    for scene in SCENES:asset('Assets/Scenes/'+scene+'.nojobscene')
    for name in MODELS:asset('Assets/Neural/'+name+'.njn')
    for name in SCRIPTS:
        for suffix in ['.cpp','.h']:asset('Assets/Scripts/'+name+suffix)
    asset('Assets/Models/testplane_100x100.obj')
    for name in ['README.md','LICENSE','THIRD_PARTY_NOTICES.md','Resources/NoJobEngine.ico','docs/images/NJE_logo.png']:copy(ROOT/name,name)
    (output/'NJE.nojobproject').write_text('NOJOB_PROJECT 1\n"NJE Alpha 1"\n"Assets"\n"Assets/Scenes/RLObstacleLearningDemo.nojobscene"\n')
    (output/'NJE.alpha').write_text('NOJOB_PUBLIC_ALPHA 1\nV2.1 / Public Alpha 1\n')
    # App-local Microsoft C++ runtime is included; no developer tool installation is needed to launch.
    if not crt.is_dir():raise FileNotFoundError('Supply the x64 Microsoft.VC14*.CRT redist directory.')
    for file in crt.glob('*.dll'):copy(file,file.name)
    # Gather authoritative upstream license texts, including Assimp's bundled components.
    for name in ['glfw','glm','assimp','glad','stb','miniaudio','imgui','imguizmo','joltphysics']:
        source=Path(cache.get('FETCHCONTENT_SOURCE_DIR_'+name.upper()) or build/'_deps'/(name+'-src'))
        candidates=[]
        for file in source.rglob('*'):
            if not file.is_file():continue
            leaf=file.name.lower()
            if leaf.startswith(('license','licence','copying','notice','copyright')):
                if file.stat().st_size<2_000_000:candidates.append(file)
        if not candidates:raise FileNotFoundError('Missing upstream license for '+name)
        for file in candidates:
            relative=file.relative_to(source).as_posix().replace('/','__')
            copy(file,'Licenses/'+name+'/'+relative+'.txt')
    sdk=Path(cache.get('NJE_WINSPARKLE_ROOT') or ROOT/'out/tooling/WinSparkle-0.9.4')
    if (build/'WinSparkle.dll').exists():
        for name in ['COPYING','COPYING.expat','AUTHORS']:copy(sdk/name,'Licenses/WinSparkle/'+name+'.txt')
    # Verify every imported non-system DLL is in the package; pull Assimp/shared dependencies when needed.
    available={p.name.lower():p for p in build.rglob('*.dll')}
    pending=list(output.glob('*.exe'))+list((output/'RuntimeData').rglob('*.dll'));audited={}
    system=Path(os.environ.get('SystemRoot','C:/Windows'))/'System32'
    while pending:
        file=pending.pop();key=file.relative_to(output).as_posix()
        if key in audited:continue
        names=imports(file);audited[key]=names
        for name in names:
            if name.endswith(('140d.dll','140_1d.dll','ucrtbased.dll')):raise ValueError('Debug runtime leaked into '+file.name)
            shipped=output/name
            if shipped.exists():
                if name not in audited:pending.append(shipped)
                continue
            if name in available:
                copy(available[name],name);pending.append(shipped);continue
            if name.startswith(('api-ms-win-','ext-ms-')):continue
            if name.startswith(('msvcp','vcruntime','concrt','vccorlib')):raise FileNotFoundError('Missing C++ runtime '+name)
            if not (system/name).is_file():raise FileNotFoundError('Unresolved dependency '+name)
    (output/'DependencyAudit.json').write_text(json.dumps(audited,indent=2))
    entries=[]
    for file in sorted(output.rglob('*')):
        if file.is_file():entries.append({'path':file.relative_to(output).as_posix(),'bytes':file.stat().st_size,'sha256':hashlib.sha256(file.read_bytes()).hexdigest()})
    (output/'PackageManifest.json').write_text(json.dumps({'product':'NJE (NoJobEngine)','version':'2.1 / Public Alpha 1','build_version':'2.1.0.2','platform':'Windows x64','scenes':SCENES,'files':entries},indent=2))
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for file in sorted(output.rglob('*')):
            if file.is_file():z.write(file,output.name+'/'+file.relative_to(output).as_posix())
    with zipfile.ZipFile(archive) as z:
        if z.testzip():raise RuntimeError('ZIP verification failed.')
    archive.with_suffix('.zip.sha256').write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n')
    print('Package:',archive);print('Files:',len(entries)+1);print('ZIP bytes:',archive.stat().st_size)
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--build-dir',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--redist-dir',type=Path,required=True)
    args=parser.parse_args();package(args.build_dir,args.output,args.redist_dir)
