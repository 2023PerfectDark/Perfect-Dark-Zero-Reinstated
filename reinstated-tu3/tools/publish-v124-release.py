"""Publish reviewed Reinstated artifacts using Git Credential Manager, never logging credentials."""
from pathlib import Path
import argparse, json, subprocess, urllib.request, urllib.parse, http.client, hashlib

REPO='2023PerfectDark/Perfect-Dark-Zero-Reinstated'
def credentials():
    r=subprocess.run(['git','credential','fill'],input='protocol=https\nhost=github.com\n\n',capture_output=True,text=True,check=True)
    values=dict(line.split('=',1) for line in r.stdout.splitlines() if '=' in line)
    return values['password']

def request(path, token, method='GET', data=None):
    body=None if data is None else json.dumps(data).encode()
    req=urllib.request.Request('https://api.github.com/'+path, data=body, method=method,
        headers={'Authorization':'Bearer '+token,'Accept':'application/vnd.github+json',
        'X-GitHub-Api-Version':'2022-11-28','User-Agent':'Reinstated-release-backup','Content-Type':'application/json'})
    with urllib.request.urlopen(req, timeout=60) as response:
        raw=response.read()
        return json.loads(raw) if raw else None

def upload(release_id, path, token, name=None):
    conn=http.client.HTTPSConnection('uploads.github.com',timeout=60)
    route=f'/repos/{REPO}/releases/{release_id}/assets?name='+urllib.parse.quote(name or path.name)
    conn.putrequest('POST', route)
    for k,v in {'Authorization':'Bearer '+token,'User-Agent':'Reinstated-release-backup',
                'Content-Type':'application/octet-stream','Content-Length':str(path.stat().st_size)}.items():
        conn.putheader(k,v)
    conn.endheaders()
    with path.open('rb') as stream:
        while chunk:=stream.read(1024*1024): conn.send(chunk)
    response=conn.getresponse(); raw=response.read(); conn.close()
    if response.status != 201: raise RuntimeError('GitHub upload failed: '+str(response.status)+' '+raw.decode()[:300])
    return json.loads(raw)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--inspect',action='store_true');p.add_argument('--publish-v124',action='store_true')
    args=p.parse_args(); token=credentials()
    release=request('repos/'+REPO+'/releases/tags/PDZInstaller',token)
    print(json.dumps({k:release[k] for k in ['id','tag_name','name','html_url','draft','prerelease']},indent=2))
    print(json.dumps([{'id':a['id'],'name':a['name'],'size':a['size']} for a in release['assets']],indent=2))
    repo=request('repos/'+REPO,token)
    print(json.dumps({'permissions':repo.get('permissions'),'default_branch':repo['default_branch']}))
    if args.publish_v124:
        root=Path(__file__).resolve().parents[1]
        files=[root/'releases/Perfect.Dark.Zero.Reinstated.v124.zip',
               root/'releases/PDZ-Reinstated-TU3-Update.zip',root/'releases/SHA256SUMS-v124.txt']
        assert all(f.is_file() for f in files)
        # Keep the user's previous uploaded installer locally before replacing it.
        old=[a for a in release['assets'] if a['name']=='Perfect.Dark.Zero.Reinstated.v121.zip']
        for asset in old:
            backup=root/'releases/previous-published-v121.zip'
            expected=asset.get('digest','').removeprefix('sha256:')
            if not backup.exists():
                req=urllib.request.Request(asset['browser_download_url'],headers={'User-Agent':'Reinstated-release-backup'})
                with urllib.request.urlopen(req,timeout=60) as response, backup.open('wb') as out:
                    while chunk:=response.read(1024*1024): out.write(chunk)
            assert backup.stat().st_size==asset['size']
            if len(expected)==64: assert hashlib.file_digest(backup.open('rb'),'sha256').hexdigest()==expected
            print('Previous installer backed up locally.',flush=True)
        uploaded=[]
        for path in files:
            digest=hashlib.file_digest(path.open('rb'),'sha256').hexdigest()
            matches=[a for a in release['assets'] if a['name']==path.name]
            if matches and matches[0].get('digest')=='sha256:'+digest:
                asset=matches[0]
            else:
                print('Uploading '+path.name,flush=True)
                asset=upload(release['id'],path,token, path.name+'.verified' if matches else None)
            assert asset['size']==path.stat().st_size and asset.get('digest')=='sha256:'+digest, asset['name']
            if matches and asset['id']!=matches[0]['id']:
                request('repos/'+REPO+'/releases/assets/'+str(matches[0]['id']),token,'DELETE')
                asset=request('repos/'+REPO+'/releases/assets/'+str(asset['id']),token,'PATCH',{'name':path.name})
            uploaded.append(asset)
            print('Verified '+path.name,flush=True)
        body=(root/'staging/v124-release-body.txt').read_text(encoding='utf-8')
        request('repos/'+REPO+'/releases/'+str(release['id']),token,'PATCH',
                {'name':'Perfect Dark Zero Reinstated 1.8 — Installer Update v124',
                 'body':body,'make_latest':'true','draft':False,'prerelease':False})
        for asset in old:
            request('repos/'+REPO+'/releases/assets/'+str(asset['id']),token,'DELETE')
            print('Replaced '+asset['name'],flush=True)
        latest=request('repos/'+REPO+'/releases/latest',token)
        assert latest['id']==release['id']
        (root/'staging/v124-published.json').write_text(json.dumps({'release':latest['html_url'],
            'assets':[{'id':a['id'],'name':a['name'],'digest':a['digest'],'url':a['browser_download_url']} for a in uploaded]},indent=2))
        print('Published and confirmed as Latest: '+latest['html_url'],flush=True)
