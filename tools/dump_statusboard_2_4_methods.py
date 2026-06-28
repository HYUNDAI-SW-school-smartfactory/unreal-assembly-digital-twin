import json, os
import unreal
MAP_PATH='/Game/Maps/MainMaps'
OUT_PATH=os.path.abspath(os.path.join(os.path.dirname(__file__),'..','artifacts','statusboard_2_4_methods.json'))
unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
ids=[]
for desc in unreal.WorldPartitionBlueprintLibrary.get_actor_descs():
    if str(desc.label)=='GenesisFactoryStatusBoard_2_4':
        ids.append(desc.guid)
if ids:
    unreal.WorldPartitionBlueprintLibrary.load_actors(ids)
actor=None
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    try:
        label=a.get_actor_label()
    except Exception:
        label=a.get_name()
    if label=='GenesisFactoryStatusBoard_2_4':
        actor=a
        break
result={}
if actor:
    result['class']=str(actor.get_class().get_path_name())
    result['name']=actor.get_name()
    result['label']=actor.get_actor_label()
    result['location']=[actor.get_actor_location().x,actor.get_actor_location().y,actor.get_actor_location().z]
    result['methods']=[m for m in dir(actor) if 'transform' in m.lower() or 'location' in m.lower() or 'root' in m.lower() or 'property' in m.lower()]
    result['components']=[]
    for comp in actor.get_components_by_class(unreal.SceneComponent):
        item={'name':comp.get_name(),'class':str(comp.get_class().get_path_name())}
        for prop in ['relative_location','relative_rotation','relative_scale3d','mobility']:
            try:
                item[prop]=str(comp.get_editor_property(prop))
            except Exception as e:
                item[prop]='ERR '+str(e)
        result['components'].append(item)
with open(OUT_PATH,'w',encoding='utf-8') as f:
    json.dump(result,f,ensure_ascii=False,indent=2)
print('Wrote '+OUT_PATH)
