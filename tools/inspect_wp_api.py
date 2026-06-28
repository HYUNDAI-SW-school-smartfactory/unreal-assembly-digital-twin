import os
import unreal
out=os.path.abspath(os.path.join(unreal.Paths.project_dir(),'..','artifacts','wp_api.txt'))
items=[]
for name in dir(unreal):
    if 'WorldPartition' in name or 'EditorActor' in name:
        items.append(name)
with open(out,'w',encoding='utf-8') as f:
    f.write('\n'.join(items))
unreal.log('WP api written '+out)
