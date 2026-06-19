# 主線事件字串萃取（events emit，grounded in bytecode）

> 產生工具：`opendw_remake/tools/extract/mainline_events.cpp`（BundleProvider 供 op_58 跨資源 call，攔截 VM message sink）。
> 跑法：`mainline_events assets/bundle`（預設主線必經區：0/1/2/4/5/6/8/14/17/20/21/23/25/27/29/30/31/32）。
> 唯一字串 210 條（已濾雜訊："?" number-sink、"Read paragraph " 觸發前綴、op_79 解碼漂移碎片）。
> **用途**：events.tsv 在地化的英文鍵來源 + quest gate halt 觀測。每條 = VM 逐條 emit（事件文字以單條為 tr 鍵）。

## quest gate halt opcode（卡住格 → 未實作 opcode）

> **更新（2026-06-16）**：op_79（DRAGON.COM 反組譯）+ op_5B（opendw 對拍）已實作並掛 dispatch。
> 下表「主線影響」欄保留歷史卡點記錄;**op_79×15 / op_5B×3 卡點已全部解除**(重跑 emit 不再 halt)。

| opcode | 卡住格數 | 語意（OPCODE_REFERENCE） | 主線影響（更新後狀態） |
|---|---|---|---|
| op_6B | 26 | 世界圖座標/移動前置（推測） | 全在 area 0 世界圖；app 走獨立 `worldmap_dest` 靜態路徑進城，**非主線阻斷**（仍未實作,不影響） |
| op_79 | ~~15~~ → **0** | set_msg 帶參數變體 = **draw_pattern + op_7A**（資料資源字串 emit;DRAGON.COM 反組譯逆出） | **已解除**：area 1/2/6/8/17/29 的 15 格重跑全部 emit 文字、不再 halt（見下節「實作後重跑」） |
| op_5B | ~~3~~ → **0** | get_map_tile_data（opendw 有 body → 對拍移植） | **已解除**：area 5/27/30 各 1 格不再 halt |

### op_79 實作後重跑（DRAGON.COM 0x47FA 反組譯）

`mainline_events assets/bundle 1 2 6 8 17 29` 重跑:**halt opcode 分佈 = 空**(15 格 op_79 全消)。
唯一字串自 53 → 72 條(area 8/29 等城內商店/酒館/募兵對話原本被 op_79 擋住,現可 emit)。
新 emit 例(area 29 軍營 tile 0x08):`"The black marketeer looks at your equipment…"` + `"Who will enter?"`;
area 8 黃泥蟾蜍 tile 0x0D:`""Welcome to the Cavern Tavern, folks," says the barkeep"`、tile 0x0F:
`"Magical Mud Inc., we'll ooze your pains away for a price!"`。語意合理、與場景一致 → **高信心**。

> **op_79 反組譯語意（@0x47FA,opendw targets[] 標 NULL → 無 C oracle）**:
> ```
> 47FA: push si / push cs / pop es / call 0x3380(draw_pattern) / pop si
>       ↓ 落入 op_7A(@0x4801)
> 4801: bx=[0x3AE2](資料資源偏移) / cx=[0x3ADF](資料資源段)
>       / call 0x1C79(extract_string) / [0x3AE2]=bx(下一條起點)
> ```
> 即 **op_79 = draw_pattern + op_7A**,與「op_77 = draw_pattern + op_78」對稱
> (op_77@0x47E3 同樣 call 0x3380 後落入 op_78@0x47EC)。draw_pattern(0x3380)純 render
> 副作用(重繪 viewport + 設 dirty),不消耗 operand、不 emit;故 remake 的 op_79
> 等同既有 op_7A(`op7A_emit_data_string`),draw_pattern 略過(與 op_77≡op_78 同處理)。

> **重要更正**：原 docs/54 推測「菲巴斯(6)/拜占儂(9)子區進入 halt 於 op_79」**有誤**。
> op_79 實作後,母區(8/29)的進入格事件**全部跑完且不寫 gs[2]**(不換區);被 op_79
> 擋住的其實是**城內商店/對話文字**,不是子區進入鏈。6/9 的進入機制另有問題,見 docs/54 更新。

## 各主線區 emit 字串（雜訊已濾）

### area 0 「Dilmun」 32x47  res=0x46
- Mad screaming rings in your ears!
- Enter Purgatory
- ?
- Enter Slave Camp
- ?
- Guard bridge ahead, approach
- ?
- Enter ancient ruins
- ?
- Heavily guarded bridge ahead, approach
- ?
- Bridge ahead, approach
- ?
- Bridge ahead, approach
- ?
- Enter the City of the Yellow Mud Toad
- ?
- Enter Smuggler's Cove
- ?
- Bridge ahead, approach
- ?
- Enter ruined city
- ?
- Splash!!
- Approach Old Dock
- ?
- Approach Pilgrim's Dock
- ?
- Enter Royal Game Preserve
- ?
- Enter decaying city
- ?
- Enter Kingshome
- ?
- Ruins ahead, Approach
- ?
- Magical forest ahead, Enter
- ?
- Enter sunken ruins
- ?
- Enter strange building
- ?
- Enter Dragon Valley
- ?
- Enter Nisir
- ?
- Enter Lansk
- ?
- Enter Byzanople
- ?
- Enter Freeport
- ?
- Enter Slave Estate
- ?
- Enter Camp
- ?
- "egin a new gameContinue 
- "egin a new gameContinue 
- "egin a new gameContinue 
- Hidden away beside this rock is an arms cache!
- You stand beside a magical pond, shall you wade in?
- "egin a new gameContinue 
- A group of guards ambush you using powerful magic. "Remember, Namtar wants them alive," is the last thing you hear before....
- "egin a new gameContinue 
- "egin a new gameContinue 
-  recognizes this area as a transportation nexus. Do you wish to teleport to the Mystic Wood?

### area 1 「Purgatory」 34x34  res=0x47
- You smell the sea. This section of wall must border on the harbor.
- Stripped of all possessions and wealth, you've been dropped naked and defenseless into the slums of Purgatory by order of Namtar, the Beast From The Pit.
- There's a gap in the city wall. Far below, you see the water of the harbor through which you entered this dreaded isle. 
- Is freedom from Purgatory worth a long dive into what might be shallow water, then a desperate swim through the harbor?
- The stone walls of Purgatory stand as a monument to shattered lives and broken dreams.
- "egin a new gameContinue 
- Read paragraph 3
- Do you wish to enter the Apsu waters
- ?
- Read paragraph 4
- You hear the lusty shouts of a large crowd coming from the east.
- You hear the bloodthirsty howls of a great crowd from behind the wall to the north.
- A great chorus of voices issues up from the west.
- The sea is cold and rough. Only a good swimmer has a chance out here.
- Read paragraph 5
- A breeze crawls in from the harbor, bearing a sickly stench.
- "egin a new gameContinue 
- Read paragraph 77
- Read paragraph 9
- Read paragraph 10
- Read paragraph 67
- "egin a new gameContinue 
- "egin a new gameContinue 
- "egin a new gameContinue 
- Read paragraph 94
- The crowd grows wild with the hope of more victims.
- The stone walls of Purgatory stand as a monument to shattered lives and broken dreams.
- "egin a new gameContinue 
- "egin a new gameContinue 
- "egin a new gameContinue 
- "egin a new gameContinue 
- "egin a new gameContinue 
- "egin a new gameContinue 
- Read paragraph 14
- You feel strangely energized!

### area 2 「Slave Camp」 17x16  res=0x48
- Read paragraph 63
- You hear someone singing.
- You hear moans of pain from inside the building.
- Read paragraph 68
- Read paragraph 22
- Read paragraph 88
- A wounded man lays on a pile of straw in the corner. His bandages are fresh but already stained with blood. He tosses and turns in a feverish slumber.
- An old wizard bars your path. "My home is off limits, friend," he says. "You're free go anywhere else you wish, but please respect my privacy."
- The door is locked.
- "egin a new gameContinue 
- Splash!!
- Someone looks at you from within the stone building.
- You have found a locked chest.

### area 5 「Ruins」 18x17  res=0x4B
- Corroded stones like rotting teeth jut from the wet soil, marking where the buildings of a proud city once stood. A battered signpost reads, "Weep for Tars."
-  says, "The walls of this city were not shattered by man - dragons have been here." 
- A huge slab of stone lays before you.
- This open area is in better repair than the rest of the city. Strange murals cover the walls.
- You notice signs that a party of men have passed this way.
- Splash!!
- Arrrggh! A Pit!!
- "egin a new gameContinue 
- "egin a new gameContinue 
- "egin a new gameContinue 

### area 6 「Phoebus」 18x18  res=0x4C
- Citizens avoid walking on this side of the street.
- "egin a new gameContinue 
- This street is remarkably clean.
- You hear the sound of soldiers marching from over the wall.
- This is a military parade grounds. A legion of spearmen perform intricate drills beneath the eyes of stern officers.
- Read paragraph 28
- Judging by the ruins here, the city seems to have been on the losing end of a war.
- "egin a new gameContinue 
- "egin a new gameContinue 
- "egin a new gameContinue 
- "egin a new gameContinue 
- "egin a new gameContinue 
- "egin a new gameContinue 
- "egin a new gameContinue 
- You have found a locked chest.
- You have found a locked chest.
- You have found a locked chest.
- "egin a new gameContinue 
- Shadowy figures welcome you to a darkened room. Few words are spoken as you are provided with simple but effective disguises. "Word of your escape will quickly reach the Stosstrupen," one of them 
- whispers. "Leave the city as soon as you are able, and do not return!"
- "egin a new gameContinue 
- A sign hangs loosely from its hinges reading, "The Icarian Triumph Tavern."

### area 8 「Mud Toad」 17x17  res=0x4E
- Read paragraph 20
- Crumbling ruins mark where the wall has been breached by the engines of war.
- Loose rubble blocks the way between the walls.
- Read paragraph 17
- Mud is slowly oozing from a fissure here.
- Read paragraph 32
- You find the city militia's weapons cache!
- "egin a new gameContinue 

### area 17 「Freeport」 17x16  res=0x57
- Splash!!
- A sign on the door reads, "
- Tars City Council."
- Read paragraph 56
- A sign on the door reads, "
- Freeport City Council."
- Read paragraph 57
- This bridge helps shelter the inner harbor.
- Read paragraph 27
- "egin a new gameContinue 
- A sign on the door reads, "
- The Temple Faith."
- A sign on the door reads, "
- Ryan's Armor."
- A sign on the door reads, "
- Freeport Arms."
- A sign on the door reads, "
- The Brew's Brothers."
- A sign on the door reads, "
- Bewitching Potions and Elixers."
- A sign on the door reads, "
- Magic Inc."
- A sign on the door reads, "
- The Order of the Sword."
- Read paragraph 52

### area 23 「Mystic Wood」 16x17  res=0x5D
- You find an odd harvest of mushrooms beneath this tree.
- Inscribed on a standing stone you read, "In memory of our Master Zaton. May his soul rest in peace."
-  recognizes this glade as a transportation nexus. Do you wish to teleport to...King's IslandQuag
- You see footprints in the mud.
- You stand before a large open glade.
- There's a simple shrine on the island in the midst of a still pond.
- A simple shrine occupies this island.
- Read paragraph 6
- A magical horn rests at the foot of this statue.
- From this well issue the cries of lost souls.
- The shore is closest here.
- "egin a new gameContinue 
- You have found a locked chest.
- "egin a new gameContinue 
- "egin a new gameContinue 
- "egin a new gameContinue 

### area 20 「Lansk」 18x17  res=0x5A
- A sign over the door reads, "
- Governor's Office."
- A sign over the door reads, "
- Visitor's Registration Department."
- A sign over the door reads, "
- Visitor's Information Bureau."
- A sign over the door reads, "
- Quarter Masters Office."
- A sign over the door reads, "
- Department of Lubrication."
- A sign over the door reads, "
- Office of the Bureau of Departments."
- Read paragraph 35
- You feel a vibration beneath your feet, as if something is moving under the surface of the city.
- A Lansk official smiles at you. 
- "Ah! You're the Outlanders," he says. "It is well that you came here. All visitors are required to register." The official points to a chart on the wall, reading aloud what is written there.
- A Lansk official smiles at you. 
- "I was wondering when you'd arrive. We've just gotten word from the Isle of Forlorn. Slaveholder Mog has died, and his will leaves his entire estate to you!" He holds up a hand when you press him with questions. "There isn't anything more I can tell you."
- There doesn't seem to be anyone here. A thick coat of dust lays over everything.
- A Lansk official smiles at you. 
- "Ah! Outlanders. You're probably here for a Governor's pass," he says. He hands you a stack of papers. "Have these stamped at the Office of Interior Affairs."
- A Lansk official smiles at you. 
- "Ah! I heard there were Outlanders in town. Do you have your visitor's registration papers?" he asks. The official smiles again awaiting your reply.
- A Lansk official smiles at you. 
- "Papers, please," he says, holding out his hand to you.
- You have found a locked chest.
- "egin a new gameContinue 

### area 14 「Necropolis」 17x17  res=0x54
- Splash!!
- "egin a new gameContinue 
- The owner of this mansion is a lover of art.
- "egin a new gameContinue 
- "egin a new gameContinue 
- This room is full of sticky spider webs.
- Ahead you see a portal of power -- wildly shifting colors swirl within a gate of arcane stone.
- Zap!!!
- There are a\de\scending stairs here. Do you wish to take them?
- "egin a new gameContinue 

### area 25 「Kingshome」 17x15  res=0x5F
- Guards stand before the entrance to the royal palace.
- Royal Guards prevent you from entering Kingshome. "Move along, Outlanders!"
- Read paragraph 130
- This is a gallery of family portraits. At the head of the gallery is a huge painting of King Drake, looking very fit and authoritarian. On the facing wall of the 
- gallery you see portraits of Drake's children Prince Jordan of Byzanople, and the twin princess Myrilla and Myrolla.
- Fine tapestries and works of art are heaped in the corner. A treasure chest full of gold stands open in the center of the room. Silverware and other luxurious 
- items are scattered across the floor. Whoever piled these valuables here either has no respect for wealth, or no need of the same.
- This is the king's wardrobe. Here you find innumerable breeches, capes, boots, and blouses, and all manner of clothing accessories. Although this collection is valuable, it would be recognized instantly if someone tried to sell it.
- In a closed trunk, you find several dozen simple white robes such as pilgrims wear.
- This is a library, long dusty from disuse. This entire wing of the palace doesn't see much use nowadays. Books are available on all variety of subjects concerning Oceana both past and present, but mostly past.
- There are a\de\scending stairs here. Do you wish to take them?
- Several items of interest lay strewn about.

### area 32 「Dragon Valley」 16x16  res=0x66
- You hear the distant rumble of a dragon... the likes of which man has never seen.
- The air reeks with the smell of death and burning dragon.
- Dragon roars are constant, now, as the brood both mourns the passing of their brothers and prepare themselves for war.
- Another baby killed.
- A wave of telepathic hate washes over you. These children had a mother...
- The air is so thick with death and tension it feels like being underwater.
- What will you do for an encore?
- "egin a new gameContinue 
- "egin a new gameContinue 
- Nestled amid the rocks you find a dragon's skeleton.
- Read paragraph 134
- You have found a locked chest.
- "egin a new gameContinue 
- "egin a new gameContinue 

### area 31 「Magic College」 8x8  res=0x65
- This chamber is bisected by a wall of fire. The heat is overwhelming. On the far side of the fire, you can see the exit from this room.
- Read paragraph 141
- Read paragraph 142
- You approach the gargoyle and it does a stoning gaze at the party... Your bodies stiffen and harden as you turned into stone...
- Read paragraph 143
- "egin a new gameContinue 
- Read paragraph 144
- You step onto the tripwire and as logic would dictate, the granite block smashs the party into unrecognizable gore.
- Read paragraph 145
- Utnapishtim is standing to the 
- north.
- Utnapishtim is standing to the 
- west.
- Read paragraph 146

### area 21 「Sunken Ruins」 9x9  res=0x5B
- There is an open well here, there appears to be buildings in the water below.
- Something in this muck-covered ground catches your eye.

### area 30 「Game Preserve」 16x16  res=0x64
- A natural ford in the river.
- In the bushes by the ford, you find animal tracks.
- "egin a new gameContinue 
- You've been snared! You hang upside down by your heels.
- Read paragraph 96
- "egin a new gameContinue 

### area 29 「Siege Camp」 16x16  res=0x63
- "egin a new gameContinue 
- "Recruits are always welcome in the service of Namtar," he says. Do you wish to join the army?
- "Then get lost."
- Read paragraph 87
- Read paragraph 90
- A Priest of the Universal God blesses you as you pass. "Namtar protect you," he says.
- A crack in the rock shows the camp where the invading army is stationed. 
- You find where a soldier has stashed his arms and armor!
- You have found a locked chest.
- There is a hidden passage here. Do you wish to take it?
- "egin a new gameContinue 

### area 4 「Salvation」 16x16  res=0x4A
- There are a\de\scending stairs here. Do you wish to take them?
- "egin a new gameContinue 
- A wise man might find a way through this rock.
- Read paragraph 55
- Read paragraph 100
- All is darkness and confusion! Where are you?
- "No one uses the front door fool!"
- You're falling!! Where did the floor go??
- Any attempt to continue here will result in certain death.
- You are falling!!
- "egin a new gameContinue 
- You have found a locked chest.

### area 27 「Depths of Nisir」 32x32  res=0x61
- This natural cavern must lay at the very heart of the universe.
- You fall for many miles and through many worlds, then find yourself sitting on wet rock. The air has a musty odor.
- The icy winds of despair tear at your soul!
- The icy winds of despair tear at your soul!
- Arrrggh! A Pit!!
- Surely this must be hell... you will never find Namtar, and Oceana will never be free!
- Oh, joy... right back where you started.
- You stand at the edge of a bottomless abyss. You dimly spot a balcony on the far side of the abyss, but there seems no way to cross. The distance is too great to jump.
- You stand at the edge of a bottomless abyss. You dimly spot a balcony on the far side of the abyss, but there seems no way to cross. The distance is too great to jump.
- A voice rings from the shadows, "So you are finally here... I must admit I underestimated you."
- A voice drifts from the darkness, "I do not care for this rampant destruction of my property... it is time you were taught a lesson!"
- Light dances oddly in the air ahead.
- The very fury of the sun seems to burn in this corridor!
- At the center of the Solarium you find Mystalvision, the renegade High Priest of the Sun. He lays on his back with his shirt off, evidently in quest of the perfect tan. He does not seem to notice your approach...
- Several dead mammals -- among them human beings -- are hung on the walls on hooks. This place looks distressingly like a meat locker.
- This chamber is heaped with the bones of hundreds of unidentifiable creatures.
- You smell a faint swampy odor from the south.
- In the heart of this mountain you find a swampy chamber filled with bubbling mud... evidently the faerie lizard folk enjoy this type of place, but you can't help but feel it is not they alone who luxuriate here.
- You've found a guard barracks... you must be on the right track.
- "egin a new gameContinue 
- "egin a new gameContinue 
- The floor moved!
- Buck Ironhead, Namtar's top general, regards you from across the room. "You've hacked past my best men -- I'm not sure what I can do against you," he says "But I'm not going down without a fight..."
- With a mad whirling and rushing you feel the stone melt away beneath your feet, transporting you to what must be another world. You find yourselves on a sun-blasted battle plain. The horizon seems an 
- uncomfortable distance away. The air is thick with swampy murk... a grim taste of what Namtar has planned for your world.
- To the south lay the forces of Namtar!
- Only fools would dare combat an army alone!
- It's you against an entire army! 
- There are a\de\scending stairs here. Do you wish to take them?
