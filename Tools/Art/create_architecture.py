"""Blender source authoring for the building roster and fitted interior shells.

Dimensions are read from the live C++ roster. Run with Blender's --background
--python. Existing source deliveries are preserved unless --replace-authored.
"""
import sys, re, math, json, hashlib
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import create_missing_props as a
import bpy
import numpy as np

ROOT=a.ROOT
ASSETS=ROOT/'Games/Zenithmon/Assets'
a.TEXTURES=ASSETS/'Buildings/_AuthoredMaterials'
a.TEXTURES.mkdir(parents=True,exist_ok=True)
render=[a.material('Lime plaster warm',(.61,.49,.34)),
        a.material('Lime plaster cool',(.43,.48,.48)),
        a.material('Ochre lime plaster',(.49,.39,.22))]
clay=a.material('Fired terracotta',(.37,.13,.065),roughness=.83)
lime=a.material('Cut limestone',(.59,.55,.43))
paint=a.material('Painted timber',(.12,.20,.19),'wood',.65)
glass=a.material('Architectural glazing',(.045,.105,.135),roughness=.13)
tile=a.material('Ceramic laboratory tile',(.64,.69,.65),roughness=.35)
copper=a.material('Weathered copper',(.17,.30,.25),'metal',.55,.75)
dark=a.material('Deep window recess',(.018,.019,.016),roughness=.93)

def window(x,y,z,w=1,h=1.25,rotation=0,lab=False):
    before=set(a.scene.objects)
    a.box('Recess shadow',(0,0,0),(w+.18,.08,h+.18),dark)
    a.box('Glazed pane',(0,.052,0),(w,.024,h),glass,.001)
    for sx in [-1,1]:
        a.box('Window jamb',(sx*(w/2+.035),.06,0),(.075,.14,h+.18),paint,.008)
    for sz in [-1,1]:
        a.box('Window rail',(0,.07,sz*(h/2+.04)),(w+.18,.14,.08),paint,.008)
    a.box('Mullion',(0,.085,0),(.04,.13,h),paint,.003)
    if not lab:a.box('Transom',(0,.085,h*.17),(w,.13,.045),paint,.003)
    a.box('Projecting stone sill',(0,.13,-h/2-.13),(w+.34,.35,.12),lime,.015)
    for ob in set(a.scene.objects)-before:
        px,py=ob.location.x,ob.location.y
        ob.location.x=x+px*math.cos(rotation)-py*math.sin(rotation)
        ob.location.y=y+px*math.sin(rotation)+py*math.cos(rotation)
        ob.location.z+=z
        ob.rotation_euler.z+=rotation

def roof(w,d,h,rise,mat,hip=False,over=.32):
    x,y=w/2+over,d/2+over
    if hip:
        ridge=max(.25,y-x*.65)
        verts=[(-x,-y,h),(x,-y,h),(x,y,h),(-x,y,h),(0,-ridge,h+rise),(0,ridge,h+rise)]
        faces=[(0,1,4),(1,2,5,4),(2,3,5),(3,0,4,5)]
        a.mesh('Hipped roof',verts,faces,mat,.012)
        for p,q in [(0,4),(1,4),(2,5),(3,5),(4,5)]:a.beam('Roof hips',verts[p],verts[q],.055,mat)
    else:
        for side in [-1,1]:
            a.mesh('Roof underlay',[(0,-y,h+rise),(side*x,-y,h),(side*x,y,h),(0,y,h+rise)],[(0,1,2,3)],mat)
        for yy in [-d/2,d/2]:
            a.mesh('Gable masonry',[(-w/2,yy,h),(w/2,yy,h),(0,yy,h+rise)],[(0,1,2)],lime)
        # Individual overlapping roof courses, with tile seams rather than a
        # single stretched roof texture. Bounded count keeps the sources editable.
        rows=max(5,int(x/.42)); columns=max(8,int(2*y/.38))
        slope=math.atan2(rise,x)
        tile_verts=[]; tile_faces=[]
        for side in [-1,1]:
            for row in range(rows):
                xx=(row+.5)*x/rows
                for col in range(columns):
                    yy=-y+(col+.5)*2*y/columns
                    sx=x/rows/math.cos(slope)+.035; sy=2*y/columns-.009
                    base=len(tile_verts); angle=side*slope
                    for dx,dy,dz in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]:
                        px,pz=dx*sx/2,dz*.047/2
                        tile_verts.append((side*xx+px*math.cos(angle)+pz*math.sin(angle),yy+dy*sy/2,
                            h+rise*(1-xx/x)+.045-px*math.sin(angle)+pz*math.cos(angle)))
                    tile_faces.extend(tuple(base+i for i in f) for f in [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)])
        a.mesh('Individually lapped roof tiles',tile_verts,tile_faces,mat,.008)
        for yy in np.linspace(-y,y,max(4,int(2*y/.42))):
            ob=a.cyl('Ridge cap',(0,float(yy),h+rise+.08),.10,.43,mat,12)
            ob.rotation_euler.x=math.pi/2
    for yy in [-y,y]:a.box('Eaves fascia',(0,yy,h-.045),(2*x,.09,.14),a.oak,.012)

def shell(w,d,h,doorw,doorh,mat):
    # Ground origin, front on Blender +Y / glTF -Z. The open door is real.
    t=.22
    for x in [-w/2+t/2,w/2-t/2]:a.box('Side wall',(x,0,h/2),(t,d,h),mat,.012)
    a.box('Rear wall',(0,-d/2+t/2,h/2),(w,t,h),mat,.012)
    wing=(w-doorw)/2
    for s in [-1,1]:a.box('Front wall',(s*(doorw/2+wing/2),d/2-t/2,h/2),(wing,t,h),mat,.012)
    a.box('Door lintel masonry',(0,d/2-t/2,(doorh+h)/2),(doorw,t,h-doorh),mat,.008)
    # Plinth courses respect the doorway aperture.
    for z in [.12,.36]:
        for s in [-1,1]:a.box('Plinth course',(s*(doorw/2+wing/2),d/2+.035,z),(wing,.14,.23),lime,.018)
        for s in [-1,1]:a.box('Side plinth',(s*(w/2+.035),0,z),(.14,d,.23),lime,.018)
    for s in [-1,1]:
        a.box('Door surround',(s*(doorw/2+.10),d/2+.06,doorh/2),(.20,.22,doorh),lime,.02)
    a.box('Door architrave',(0,d/2+.07,doorh+.13),(doorw+.50,.25,.24),lime,.02)
    for xx in [-w/2,w/2]:
        for yy in [-d/2,d/2]:
            for z in np.arange(.3,h,.46):
                a.box('Dressed corner quoin',(xx,yy,float(z)),(.35,.34,.39),lime,.018)

def building(row):
    name,w,d,wall,pitch,floors,doorw,doorh=row
    h=wall*floors
    palette=1 if ('Cool' in name or name in ['Lab','League','BattleTower','GymWater','GymSky','GymIce','GymPhantom']) else 2 if ('Earth' in name or name in ['GymGrass','GymDrake','Warehouse','Gatehouse']) else 0
    mat=render[palette]; roofing=[clay,a.slate,a.dry][palette]
    shell(w,d,h,doorw,doorh,mat)
    for floor in range(floors):
        z=floor*wall+wall*.57
        count=max(1,int((w-doorw)/3))
        for side in [-1,1]:
            for k in range(count):
                x=side*(doorw/2+(w-doorw)/2*(k+.6)/(count+.2))
                window(x,d/2+.025,z,min(1.05,(w-doorw)/2/count*.55),min(1.35,wall*.5),lab=name=='Lab')
        for side in [-1,1]:
            for yy in np.linspace(-d*.32,d*.32,max(2,int(d/3))):
                window(side*(w/2+.02),float(yy),z,1,1.25,-side*math.pi/2,lab=name=='Lab')
        if floor:a.box('Front string course',(0,d/2+.10,floor*wall),(w+.15,.22,.16),lime,.015)
    # Each landmark has a distinct structural roof and entrance silhouette.
    if name=='GymGrass':
        # A barrel-vaulted conservatory, with exposed ribs and buttresses.
        for yy in np.linspace(-d/2,d/2,12):
            pts=[(w*.5*math.cos(t),float(yy),h+w*.25*math.sin(t)) for t in np.linspace(0,math.pi,15)]
            for p,q in zip(pts,pts[1:]):a.beam('Conservatory rib',p,q,.065,copper)
        for j in range(14):
            t0,t1=j*math.pi/14,(j+1)*math.pi/14
            a.mesh('Vault glazing',[(w*.5*math.cos(t),yy,h+w*.25*math.sin(t)) for yy,t in [(-d/2,t0),(d/2,t0),(d/2,t1),(-d/2,t1)]],[(0,1,2,3)],glass)
    elif name=='GymFire':
        # Sawtooth industrial roof and asymmetric furnace chimneys.
        for yy in [-d/3,0,d/3]:
            a.mesh('Sawtooth roof',[(-w/2,yy-d/6,h),(w/2,yy-d/6,h),(w/2,yy+d/6,h+2),(-w/2,yy+d/6,h+2)],[(0,1,2,3)],a.iron)
        for xx,extra in [(-w*.34,4),(w*.34,2.6)]:
            a.box('Furnace chimney',(xx,-d*.3,h+extra/2),(1.2,1.2,extra),clay,.06)
            a.box('Chimney coping',(xx,-d*.3,h+extra),(1.45,1.45,.25),a.basalt,.03)
    elif name=='GymWater':
        for side in [-1,1]:
            a.mesh('Swept wave roof',[(side*x,yy,h+1.7*math.sin(x/w*math.pi)) for x,yy in [(0,-d/2),(w/2,-d/2),(w/2,d/2),(0,d/2)]],[(0,1,2,3)],copper)
        for xx in [-w*.32,w*.32]:
            a.cyl('Water tower',(xx,-d*.31,h+1.2),1.4,2.4,lime,48)
            a.sphere('Domed tank',(xx,-d*.31,h+2.4),(2.9,2.9,1.4),copper,4)
    elif name=='GymElectric':
        a.box('Substation roof',(0,0,h+.12),(w+.4,d+.4,.24),a.iron)
        for xx in [-w*.36,w*.36]:
            for yy in [-d*.30,d*.3]:
                a.beam('Pylon mast',(xx,yy,h),(xx*.7,yy*.7,h+4),.1,a.iron)
            a.beam('Pylon crossbar',(xx*.7,-d*.21,h+4),(xx*.7,d*.21,h+4),.12,a.iron)
            for yy in [-d*.17,0,d*.17]:
                for z in np.linspace(h+3.55,h+4.2,6):a.cyl('Ceramic insulator',(xx*.7,yy,float(z)),.23,.07,tile,16)
    elif name=='GymSky':
        for side in [-1,1]:
            a.mesh('Wing canopy',[(0,-d/2,h+.3),(side*w*.65,-d/2,h+2.5),(side*w*.65,d/2,h+1.7),(0,d/2,h+.3)],[(0,1,2,3)],lime)
        a.cyl('Observation drum',(0,-d*.18,h+1.6),1.6,3.2,lime,48)
        a.sphere('Observatory dome',(0,-d*.18,h+3.2),(3.7,3.7,2),copper,4)
    elif name=='GymPhantom':
        roof(w,d,h,3.8,a.slate,False)
        for xx in [-w*.4,w*.4]:
            a.box('Bell tower',(xx,d*.36,h+1.3),(1.8,1.8,2.6),a.basalt,.03)
            bpy.ops.mesh.primitive_cone_add(vertices=8,radius1=1.45,depth=3,location=(xx,d*.36,h+4.1))
            a.finish(bpy.context.object,'Gothic spire',a.slate)
    elif name=='GymIce':
        for xx in [-w*.34,0,w*.34]:
            before=set(a.scene.objects)
            roof(w/3,d,h,3,lime,False,.12)
            for ob in set(a.scene.objects)-before:ob.location.x+=xx
        for xx in [-w*.38,0,w*.38]:
            for side in [-1,1]:a.beam('Ice hall portal rib',(xx+side*w*.16,d/2+.3,h),(xx,d/2+.3,h+3),.16,lime)
    elif name=='GymDrake':
        roof(w,d,h,2.5,a.slate,True)
        for xx in [-w*.4,w*.4]:
            a.cyl('Fortress turret',(xx,d*.37,h*.62),1.3,h*1.24,a.basalt,32)
            for theta in np.linspace(0,2*math.pi,10,endpoint=False):
                a.box('Turret merlon',(xx+1.1*math.cos(theta),d*.37+1.1*math.sin(theta),h*1.24+.3),(.5,.5,.6),a.basalt,.035)
    elif name=='CareCenter':
        roof(w,d,h,1.4,clay,True)
        a.cyl('Central treatment lantern',(0,0,h+1.8),1.35,2,lime,48)
        a.sphere('Rounded lantern cap',(0,0,h+2.8),(3.1,3.1,1.3),copper,4)
        a.box('Deep sheltered portico',(0,d/2+.75,doorh+.5),(w*.72,1.8,.3),lime,.05)
    elif name in ['League','TownHall']:
        roof(w,d,h,2.4 if name=='League' else 1.5,a.slate,True)
        for xx in np.linspace(-w*.4,w*.4,6):
            if abs(xx)<doorw/2+.25:continue
            a.cyl('Portico column',(float(xx),d/2+.65,h*.48),.22,h*.96,lime,32)
        a.mesh('Classical pediment',[(-w*.47,d/2+.9,h),(w*.47,d/2+.9,h),(0,d/2+.9,h+2)],[(0,1,2)],lime,.02)
    elif name=='BattleTower':
        for z in np.arange(h,h+7,2):
            size=w*(1-(z-h)/12)
            a.box('Stepped tower tier',(0,0,float(z)+.8),(size,size,1.6),mat,.06)
            a.box('Tier cornice',(0,0,float(z)+1.7),(size+.35,size+.35,.25),lime,.03)
        a.beam('Tower finial',(0,0,h+7),(0,0,h+9),.09,copper)
    elif name in ['Lab','Warehouse','DockShed','TradePost'] or name.startswith('Shop'):
        a.box('Flat roof',(0,0,h+.12),(w+.24,d+.24,.24),a.slate,.025)
        for side in [-1,1]:a.box('Parapet',(side*w/2,0,h+.35),(.18,d,.5),mat,.015)
        a.box('Blank shop fascia',(0,d/2+.14,h-.30),(w*.92,.25,.52),paint,.015)
        if name=='Lab':
            for xx in [-w*.23,w*.23]:
                a.box('Raised rooflight',(xx,-d*.12,h+.48),(w*.26,d*.45,.55),glass,.03)
                for yy in np.linspace(-d*.345,d*.105,7):a.box('Rooflight glazing bar',(xx,float(yy),h+.77),(w*.26,.06,.06),a.iron,.004)
            for xx in [-w*.40,w*.40]:a.cyl('Ventilation cowl',(xx,-d*.3,h+.65),.35,1,copper)
    else:
        roof(w,d,h,min(w*.5*pitch,3.2),roofing,name.startswith('Townhouse'),.65 if name.startswith('Chalet') else .3)
        a.box('Chimney',(-w*.29,-d*.23,h+.9),(.68,.68,1.8),lime,.025)
        a.box('Chimney cap',(-w*.29,-d*.23,h+1.85),(.85,.85,.18),a.slate,.015)
        if name.startswith('Chalet'):
            a.box('Balcony deck',(0,d/2+.50,wall),(w*.8,1.2,.16),a.oak,.015)
            for xx in np.linspace(-w*.4,w*.4,13):a.box('Balcony baluster',(float(xx),d/2+1,wall+.5),(.065,.075,1),a.oak,.006)
            a.box('Balcony handrail',(0,d/2+1,wall+1),(w*.85,.12,.10),a.oak,.015)
    # Gutters and downpipes carry scale/detail at human viewing distance.
    for side in [-1,1]:
        a.beam('Rainwater downpipe',(side*(w/2-.16),d/2+.20,.15),(side*(w/2-.16),d/2+.20,h),.055,copper)

def interior(name):
    # Dimensions resolved from the same pure placement contract as C++.
    file=ROOT/f'Games/Zenithmon/Source/World/ZM_{name}Placement.h'
    text=file.read_text()
    prefix='PLAYERHOME' if name=='PlayerHome' else 'PROFLAB'
    def constant(s):
        m=re.search(r'fZM_'+prefix+'_'+s+r'\s*=\s*([\d.]+)f',text)
        if not m:raise ValueError('Missing live room constant '+prefix+'_'+s)
        return float(m[1])
    # Filled by the named mappings below: fail closed if C++ changes its contract.
    w,d=2*constant('HALF_WIDTH'),2*constant('HALF_DEPTH')
    h=constant('WALL_HEIGHT'); dw=constant('APERTURE_HALF_WIDTH')*2; dh=constant('APERTURE_HEIGHT')
    mat=render[0] if name=='PlayerHome' else tile
    # Shell uses real thickness but all visible faces are on the inward envelope.
    a.box('Floor substrate',(0,0,-.10),(w,d,.20),a.oak if name=='PlayerHome' else tile)
    if name=='PlayerHome':
        for i,x in enumerate(np.arange(-w/2+.12,w/2,.24)):
            for j in range(4):a.box('Floorboard',(float(x),-d/2+(j+.5)*d/4,.007),(.232,d/4-.008,.014),a.oak,.003)
    else:
        for x in np.arange(-w/2+.4,w/2,.8):
            for y in np.arange(-d/2+.4,d/2,.8):a.box('Floor ceramic',(float(x),float(y),.009),(.788,.788,.018),tile,.003)
    for side in [-1,1]:a.box('Interior side wall',(side*(w/2+.1),0,h/2),(.2,d,h),mat,.004)
    a.box('Interior rear wall',(0,d/2+.1,h/2),(w,.2,h),mat,.004)
    wing=(w-dw)/2
    for side in [-1,1]:a.box('Entrance wing',(side*(dw/2+wing/2),-d/2-.1,h/2),(wing,.2,h),mat,.004)
    a.box('Entrance lintel',(0,-d/2-.1,(h+dh)/2),(dw,.2,h-dh),mat,.004)
    a.box('Ceiling',(0,0,h+.06),(w,d,.12),render[1],.004)
    for side in [-1,1]:
        a.box('Side skirting',(side*(w/2-.035),0,.11),(.07,d,.22),paint,.008)
        a.box('Cornice',(side*(w/2-.065),0,h-.12),(.13,d,.18),lime,.012)
        for y in np.linspace(-d*.35,d*.35,4):window(side*(w/2-.05),float(y),h*.61,1.5,1.3,side*math.pi/2,lab=name=='ProfLab')
    a.box('Rear skirting',(0,d/2-.04,.11),(w,.08,.22),paint,.006)
    for side in [-1,1]:a.box('Entrance reveal',(side*(dw/2+.06),-d/2+.035,dh/2),(.12,.12,dh),paint,.008)
    a.box('Entrance header',(0,-d/2+.035,dh+.07),(dw+.24,.12,.14),paint,.008)
    if name=='PlayerHome':
        for x in np.linspace(-w*.35,w*.35,4):a.box('Exposed ceiling joist',(float(x),0,h-.10),(.14,d,.20),a.oak,.015)
    else:
        for y in np.linspace(-d*.35,d*.35,5):a.box('Laboratory ceiling channel',(0,float(y),h-.04),(w,.08,.08),paint,.005)

def export(family,name,build):
    target=ASSETS/family/name
    if (target/(name+'.glb')).exists() and '--replace-authored' not in sys.argv:return
    target.mkdir(parents=True,exist_ok=True)
    a.scene=bpy.data.scenes.new(f'{family} {name}'); bpy.context.window.scene=a.scene
    a.scene.unit_settings.system='METRIC'
    a.rng=np.random.default_rng(int.from_bytes(hashlib.sha256(name.encode()).digest()[:8],'little'))
    build()
    bpy.ops.object.select_all(action='SELECT'); bpy.context.view_layer.objects.active=next(iter(a.scene.objects))
    bpy.ops.object.join(); ob=bpy.context.object; ob.name=name
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    mod=ob.modifiers.new('Export triangles','TRIANGULATE'); bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.ops.export_scene.gltf(filepath=str(target/(name+'.glb')),export_format='GLB',use_selection=True,
        use_active_scene=True,export_animations=False,export_tangents=True,export_yup=True)
    bpy.data.libraries.write(str(target/(name+'.blend')),{a.scene},fake_user=True)
    print('AUTHORED',family,name,len(ob.data.polygons),'triangles',flush=True)

def main():
    only=next((v.split('=',1)[1].split(',') for v in sys.argv if v.startswith('--only=')),None)
    text=(ROOT/'Games/Zenithmon/Source/Data/ZM_BuildingData.cpp').read_text()
    for line in text.splitlines():
        if not re.search(r'\{ ZM_BUILDING_',line):continue
        fields=[v.strip() for v in line.strip().strip('{},').split(',')]
        name=fields[1].strip('"')
        if only and name not in only:continue
        row=(name,*[float(fields[i].rstrip('f')) for i in [5,6,7,8]],int(fields[9].rstrip('u')),
             float(fields[14].rstrip('f')),float(fields[15].rstrip('f')))
        export('Buildings',name,lambda r=row:building(r))
    for name in ['PlayerHome','ProfLab']:
        if not only or 'Interior'+name in only:export('Interiors',name,lambda n=name:interior(n))

if __name__=='__main__':main()
