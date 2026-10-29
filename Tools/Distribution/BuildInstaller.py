"Compile an NJE per-user installer and prepare a signed publication repository."
from pathlib import Path
import argparse,subprocess,shutil,re,json,hashlib,datetime,xml.etree.ElementTree as ET,zipfile
ROOT=Path(__file__).resolve().parents[2]
NS='http://www.andymatuschak.org/xml-namespaces/sparkle'
ET.register_namespace('sparkle',NS)
def invoke(args):
    result=subprocess.run([str(a) for a in args],capture_output=True,text=True,errors='replace')
    if result.returncode:raise RuntimeError(result.stdout+'\n'+result.stderr)
    return result.stdout.strip()
def build(args):
    alpha=re.search(r'#define NJE_ALPHA_VERSION "([^"]+)"',(ROOT/'Resources/Version.h').read_text()).group(1)
    stage=args.package.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    assert (stage/'NJE.exe').is_file(),'Prepare the Release package first.'
    (stage/'NJE.installed').write_text('NJE_INSTALLED 1\n')
    manifest_path=stage/'PackageManifest.json'
    manifest=json.loads(manifest_path.read_text())
    if not args.test and manifest.get('build_version')!=args.version:raise ValueError('Installer version must match the Release package build version.')
    manifest['distribution']='per-user installer'
    manifest['files']=[x for x in manifest['files'] if x['path'] not in ('NJE.alpha','NoJobEngine.alpha','NJE.installed')]
    marker=stage/'NJE.installed'
    manifest['files'].append({'path':'NJE.installed','bytes':marker.stat().st_size,'sha256':hashlib.sha256(marker.read_bytes()).hexdigest()})
    manifest_path.write_text(json.dumps(manifest,indent=2))

    config=(ROOT/'Resources/UpdateConfig.h').read_text()
    public=re.search(r'PublicKey\[\]="([^"]+)"',config).group(1)
    base=[args.iscc,'/DStageDir='+str(stage),'/DOutputDir='+str(out),'/DAppVersion='+args.version,'/DAlphaVersion='+alpha]
    if args.test:base.append('/DNJETestMode=1')
    log=invoke([*base,ROOT/'Tools/Distribution/NJE.iss']);(out/'compile.log').write_text(log)
    name=('NJE_TestSetup_'+args.version if args.test else 'NJE_Alpha_'+alpha+'_Setup')+'.exe';installer=out/name
    assert installer.is_file(),name
    if args.test:return installer
    signature=invoke([args.tool,'sign','--private-key-file',args.private_key,installer]).splitlines()[-1].strip()
    assert re.fullmatch(r'[A-Za-z0-9+/=]+',signature) and len(signature)>70,'Unexpected signing output.'
    invoke([args.tool,'verify','--public-key',public,'--signature',signature,installer])
    sha=hashlib.sha256(installer.read_bytes()).hexdigest();installer.with_suffix('.exe.sha256').write_text(sha+'  '+name+'\n')
    repository=out/'publication'/'repository';docs=repository/'docs';docs.mkdir(parents=True,exist_ok=True)
    release=out/'publication'/'release-assets';release.mkdir(exist_ok=True)
    shutil.copy2(installer,release/name);shutil.copy2(installer.with_suffix('.exe.sha256'),release/(name+'.sha256'))
    tag='alpha-'+alpha;site=f'https://{args.owner.lower()}.github.io/{args.repository}'
    download=f'https://github.com/{args.owner}/{args.repository}/releases/download/{tag}/{name}'
    rss=ET.Element('rss',{'version':'2.0'});channel=ET.SubElement(rss,'channel')
    for key,value in [('title','NJE Alpha updates'),('link',site+'/'),('description','Signed NJE Alpha releases'),('language','en')]:ET.SubElement(channel,key).text=value
    item=ET.SubElement(channel,'item');ET.SubElement(item,'title').text='NJE Alpha '+alpha
    ET.SubElement(item,'{'+NS+'}version').text=args.version
    ET.SubElement(item,'{'+NS+'}releaseNotesLink').text=site+'/release-notes-alpha-'+alpha+'.html'
    ET.SubElement(item,'{'+NS+'}minimumSystemVersion').text='10.0.17763'
    ET.SubElement(item,'pubDate').text=datetime.datetime.now(datetime.timezone.utc).strftime('%a, %d %b %Y %H:%M:%S +0000')
    ET.SubElement(item,'enclosure',{'url':download,'length':str(installer.stat().st_size),'type':'application/octet-stream','{'+NS+'}version':args.version,'{'+NS+'}shortVersionString':'Alpha '+alpha,'{'+NS+'}edSignature':signature,'{'+NS+'}os':'windows-x64','{'+NS+'}installerArguments':'/SILENT /SP- /NOICONS /NORESTART'})
    ET.indent(rss);ET.ElementTree(rss).write(docs/'alpha.xml',encoding='utf-8',xml_declaration=True)
    (docs/'.nojekyll').write_text('')
    (docs/('release-notes-alpha-'+alpha+'.html')).write_text('<!doctype html><meta charset="utf-8"><title>NJE Alpha '+alpha+'</title><h1>NJE Alpha '+alpha+'</h1><ul><li>Build Project exports with the bundled Release runtime, policies and compiled scripts without Visual Studio.</li><li>Behaviour Tree patrol/chase demo with its editable tree, automatic target and navigation bake.</li><li>Empty startup scene with a camera, two lights and editor grid.</li><li>File &gt; New Scene, save confirmation and scene shortcuts.</li><li>Per-user installation with the NJE application icon.</li><li>Projects, trained models and settings live outside the application.</li><li>Signed Alpha update checks and save-before-install.</li><li>Independent trainer and six bundled examples.</li></ul>',encoding='utf-8')
    (docs/'index.html').write_text('<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>NJE Alpha</title><style>body{font:18px system-ui;max-width:760px;margin:10vh auto;padding:24px;background:#091421;color:#edf6ff}a{color:#71dcff}</style><img src="images/NJE_logo.png" alt="NJE logo" width="180"><h1>NJE &mdash; NoJobEngine</h1><h2>Alpha '+alpha+'</h2><p>Native C++ game AI, visible reinforcement learning and live diagnostics.</p><p><a href="'+download+'">Download NJE for Windows x64</a></p><p>Windows 10/11 and OpenGL 4.6. Personal evaluation under the included license.</p><p><a href="release-notes-alpha-'+alpha+'.html">Release notes</a> &middot; <a href="alpha.xml">Alpha update feed</a></p></html>',encoding='utf-8')
    (repository/'.gitignore').write_text('# Release binaries belong in GitHub Releases, not Git history.\n*.exe\n*.msi\n*.zip\n*.7z\n*.sha256\n# Never commit signing secrets.\n*.key\n*.pem\n*.pfx\n*.p12\n*.p8\n.env\n.env.*\nprivate*\n# Local build/editor files.\nbuild/\nout/\n.vs/\n.vscode/\n__pycache__/\n*.log\nThumbs.db\n.DS_Store\n')
    (repository/'README.md').write_text((ROOT/'Tools/Distribution/ReleaseREADME.txt').read_text(encoding='utf-8').format(owner=args.owner,repository=args.repository,site=site,installer=name,alpha=alpha),encoding='utf-8')
    images=docs/'images';images.mkdir(exist_ok=True)
    shutil.copy2(ROOT/'docs/images/NJE_logo.png',images/'NJE_logo.png')
    instructions=f'''PUBLICAR NJE - no se ha subido nada automaticamente

1. Crea el repositorio PUBLICO {args.owner}/{args.repository}. Deja el repositorio de desarrollo como esta.
2. Sube el CONTENIDO de repository/ a la raiz y confirma en la rama main. No subas la carpeta contenedora repository.
3. En Releases > Draft a new release, crea el tag {tag}. Marca la version como pre-release.
4. Adjunta los DOS archivos de release-assets/. Publica esa release. El instalador va como asset de Releases, no como archivo del repositorio.
5. En Settings > Pages, elige Deploy from a branch, main y /docs. Guarda y espera al despliegue.
6. Comprueba {site}/ y {site}/alpha.xml . El enlace de descarga debe bajar {name}.
7. Instala NJE y abre Help > Check for updates. Esta version debe indicar que esta al dia.

Para futuras versiones:
- Conserva el mismo repositorio, AppId del instalador y clave de firma.
- Aumenta la version publica NJE_ALPHA_VERSION para cada Alpha nueva y el numero interno NJE_VERSION_BUILD para cada compilacion publicada; ambos estan en Resources/Version.h. Compila y prepara el paquete Release.
- Ejecuta BuildInstaller.py con --version y la clave privada local, luego repite la publicacion.
- Publica primero el nuevo instalador en su release; despues actualiza docs/alpha.xml y las notas.
- No cambies el nombre del instalador despues de generar el feed.

La clave privada NO esta en esta carpeta ni debe subirse a GitHub. Guardala y haz una copia privada de seguridad.
La firma EdDSA protege las actualizaciones de NJE. Este instalador no tiene firma Authenticode de editor Windows.
Hasta publicar la release y activar Pages, la comprobacion online no puede encontrar este canal.
'''
    (out/'publication'/'PUBLICAR_EN_GITHUB.txt').write_text(instructions,encoding='utf-8')
    with zipfile.ZipFile(out/'NJE_Publicacion_GitHub.zip','w',zipfile.ZIP_DEFLATED) as z:
        for file in (out/'publication').rglob('*'):
            if file.is_file():z.write(file,file.relative_to(out/'publication'))
    (out/'signature.json').write_text(json.dumps({'public_version':'Alpha '+alpha,'version':args.version,'sha256':sha,'signature':signature,'public_key':public,'feed':site+'/alpha.xml','download':download},indent=2))
    return installer
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for key in ['package','output','iscc','tool','private-key']:p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--version',default='.'.join(re.search(r'#define NJE_VERSION_'+part+r' (\d+)',(ROOT/'Resources/Version.h').read_text(encoding='utf-8')).group(1) for part in ['MAJOR','MINOR','PATCH','BUILD']));p.add_argument('--owner',default='Nil0s');p.add_argument('--repository',default='NJE-Releases');p.add_argument('--test',action='store_true')
    print(build(p.parse_args()))
