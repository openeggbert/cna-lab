#pragma once

#include <array>
#include <string_view>

namespace black_pine::content {

enum class Region {
    caretaker,
    relay,
    forest,
    quarry,
    railway,
    reservoir,
    mine,
    observatory,
    bunker,
    summit,
};

struct Screen final {
    int number;
    std::string_view id;
    std::string_view englishTitle;
    std::string_view czechTitle;
    std::string_view englishStory;
    std::string_view czechStory;
    Region region;
    bool travelAnchor{};
};

inline constexpr std::array<Screen, 124> screens{{
    {1, "storm_gate_trailhead", "STORM GATE TRAILHEAD", "VÝCHOZIŠTĚ U BOUŘKOVÉ BRÁNY", "A damaged toolbox holds the first repair supplies. The tower is dark beyond the storm.", "Poškozená skříňka ukrývá první vybavení k opravě. Věž je za bouří temná.", Region::caretaker, true},
    {2, "ranger_noticeboard", "RANGER NOTICEBOARD", "NÁSTĚNKA STRÁŽCŮ", "A notice reports Kestrel Six missing with an injured child aboard.", "Oznámení hlásí pohřešovaný vrtulník Kestrel Six se zraněným dítětem na palubě.", Region::caretaker},
    {3, "lower_switchback", "LOWER SWITCHBACK", "DOLNÍ SERPENTINA", "Heavy survey boots crossed the mud before the storm arrived.", "Těžké průzkumnické boty prošly blátem ještě před příchodem bouře.", Region::caretaker},
    {4, "upper_switchback", "UPPER SWITCHBACK", "HORNÍ SERPENTINA", "A fallen feeder spits blue arcs across the direct path.", "Spadlý přívod prská modrými výboji přes přímou cestu.", Region::caretaker},
    {5, "pine_hollow_footbridge", "PINE HOLLOW FOOTBRIDGE", "LÁVKA V BOROVÉM ÚVOZU", "A freshly cut cable tie bears a triangular survey mark.", "Čerstvě přeříznutá kabelová páska nese trojúhelníkovou značku průzkumníků.", Region::caretaker},
    {6, "caretaker_cabin_exterior", "CARETAKER CABIN EXTERIOR", "PŘED SPRÁVCOVSKOU CHATOU", "Mara's silhouette waits behind a rain-streaked cabin window.", "Za deštěm zmáčeným oknem chaty čeká Mařina silueta.", Region::caretaker},
    {7, "caretaker_cabin_main", "CARETAKER CABIN", "SPRÁVCOVSKÁ CHATA", "Mara knows the relay, and her desk log hides the yard key.", "Mara zná převaděč a její deník na stole ukrývá klíč od areálu.", Region::caretaker, true},
    {8, "cabin_radio_nook", "RADIO NOOK", "RÁDIOVÝ KOUT", "The receiver carries a precisely timed pulse instead of storm noise.", "Přijímač zachycuje místo bouřkového šumu přesně časovaný pulz.", Region::caretaker},
    {9, "caretaker_tool_shed", "TOOL SHED", "KŮLNA S NÁŘADÍM", "Mara's field tools remain, but the climbing rope was lent to Theo.", "Mařino polní nářadí zůstalo, ale horolezecké lano si vypůjčil Theo.", Region::caretaker},
    {10, "cabin_root_cellar", "ROOT CELLAR", "SKLEP POD CHATOU", "A ceramic fuse and crank torch rest beside an old NIGHTJAR crate.", "Keramická pojistka a ruční svítilna leží vedle staré bedny NIGHTJAR.", Region::caretaker},
    {11, "weather_mast_clearing", "WEATHER MAST CLEARING", "MÝTINA S METEOSTOŽÁREM", "The mast can calibrate a direction bearing once local power returns.", "Stožár po návratu místního napájení zkalibruje směrový náměr.", Region::caretaker},
    {12, "old_service_road_fork", "OLD SERVICE ROAD FORK", "ROZCESTÍ STARÉ SERVISNÍ CESTY", "A forestry barrier guards the northern route toward Nightjar.", "Lesnická závora střeží severní cestu k Nightjaru.", Region::caretaker},

    {13, "relay_perimeter", "RELAY PERIMETER", "OBVOD PŘEVADĚČE", "The outer alarm wire was cut cleanly and wrapped against rain.", "Vnější poplašný drát někdo čistě přeřízl a ochránil před deštěm.", Region::relay},
    {14, "vehicle_gate", "VEHICLE GATE", "VJEZDOVÁ BRÁNA", "The brass yard key fits the only lock the storm did not break.", "Mosazný klíč od areálu pasuje do jediného zámku, který bouře nezničila.", Region::relay},
    {15, "relay_yard_west", "RELAY YARD WEST", "ZÁPADNÍ ČÁST AREÁLU", "Bootprints cross a stripped grounding braid and lead to the trench.", "Stopy bot kříží odizolované uzemnění a vedou ke kabelovému výkopu.", Region::relay, true},
    {16, "relay_yard_east", "RELAY YARD EAST", "VÝCHODNÍ ČÁST AREÁLU", "Labels connect generator, transformer, trench and control room into one system.", "Štítky spojují generátor, transformátor, výkop a velín do jednoho systému.", Region::relay},
    {17, "cable_trench", "CABLE TRENCH", "KABELOVÝ VÝKOP", "The factory jumper was deliberately removed from two blue terminals.", "Někdo záměrně odstranil tovární propojku mezi dvěma modrými svorkami.", Region::relay},
    {18, "generator_shed", "GENERATOR SHED", "GENERÁTOROVNA", "The diesel set needs a main fuse and every downstream repair before it can latch.", "Dieselagregát potřebuje hlavní pojistku a všechny navazující opravy.", Region::relay},
    {19, "battery_room", "BATTERY ROOM", "AKUMULÁTOROVNA", "A loose bus link and careful footprints reveal a stolen diagnostic tape.", "Uvolněná spojnice a opatrné stopy prozrazují krádež diagnostické pásky.", Region::relay},
    {20, "fuel_pump_alcove", "FUEL PUMP ALCOVE", "VÝKLENEK PALIVOVÉHO ČERPADLA", "The seized supply valve must open before fuel can reach the generator.", "Zadřený přívodní ventil se musí otevřít, aby palivo dorazilo ke generátoru.", Region::relay},
    {21, "transformer_pad", "TRANSFORMER PAD", "TRANSFORMÁTOROVÉ STANOVIŠTĚ", "The live feeder can be isolated safely with lineman gloves.", "Živý přívod lze bezpečně odpojit v elektrikářských rukavicích.", Region::relay},
    {22, "relay_workshop", "RELAY WORKSHOP", "DÍLNA PŘEVADĚČE", "Calder's initials are scratched beneath a locked-away multimeter.", "Pod zamčeným multimetrem jsou vyškrábané Calderiny iniciály.", Region::relay},
    {23, "lower_relay_hall", "LOWER RELAY HALL", "DOLNÍ RELÉOVÝ SÁL", "A signal lives on the disconnected Nightjar trunk even without local power.", "V odpojené trase Nightjar je signál i bez místního napájení.", Region::relay},
    {24, "local_control_room", "LOCAL CONTROL ROOM", "MÍSTNÍ VELÍN", "The trace prints NIGHTJAR QUIET FIELD and a bearing of 017 degrees.", "Trasovač vytiskne NIGHTJAR QUIET FIELD a náměr 017 stupňů.", Region::relay, true},

    {25, "north_service_road", "NORTH SERVICE ROAD", "SEVERNÍ SERVISNÍ CESTA", "A discarded survey ribbon follows the suspicious bearing north.", "Odhozená průzkumnická páska sleduje podezřelý náměr k severu.", Region::forest},
    {26, "burned_pine_stand", "BURNED PINE STAND", "SPÁLENÝ BOROVÝ HÁJ", "A dragged cable crosses old ash beside a hidden ranger bandage.", "Tažený kabel kříží starý popel vedle ukrytého obvazu strážců.", Region::forest},
    {27, "fallen_fir", "FALLEN FIR", "PADLÁ JEDLE", "A huge fir blocks the road but can become a permanent step.", "Obrovská jedle blokuje cestu, ale může se změnit v trvalý schod.", Region::forest},
    {28, "cold_creek_crossing", "COLD CREEK CROSSING", "BROD PŘES STUDENÝ POTOK", "Red ranger fabric caught downstream points toward Theo.", "Červená látka strážců zachycená po proudu ukazuje k Theovi.", Region::forest},
    {29, "hunters_blind", "HUNTER'S BLIND", "LOVECKÝ POSED", "Theo's broken radio and call sign lie beside an emergency flare.", "Theovo rozbité rádio a volací znak leží vedle nouzové světlice.", Region::forest},
    {30, "mossy_hollow", "MOSSY HOLLOW", "MECHOVÁ PROHLUBEŇ", "Theo is pinned by a branch and knows where the false surveyors went.", "Thea uvěznila větev a on ví, kam falešní průzkumníci odešli.", Region::forest},
    {31, "ranger_cache", "RANGER CACHE", "SKLAD STRÁŽCŮ", "Theo's combination opens rope, hook, lamp and compass supplies.", "Theova kombinace zpřístupní lano, hák, lampu a kompas.", Region::forest, true},
    {32, "charcoal_kiln_ruin", "CHARCOAL KILN RUIN", "RUINA MILÍŘE", "Clean hardwood charcoal will later filter the mine air.", "Čisté dřevěné uhlí později přefiltruje vzduch v dole.", Region::forest},
    {33, "echo_grove", "ECHO GROVE", "HÁJ OZVĚN", "Compass and bearing 017 reveal the route hidden among identical pines.", "Kompas a náměr 017 odhalí cestu mezi stejnými borovicemi.", Region::forest},
    {34, "buried_cable_ridge", "BURIED CABLE RIDGE", "HŘBET SE ZAKOPANÝM KABELEM", "Three posts show the Nightjar leakage strengthening toward the quarry.", "Tři sloupky ukazují, že únik Nightjaru sílí směrem k lomu.", Region::forest},
    {35, "bear_meadow", "BEAR MEADOW", "MEDVĚDÍ LOUKA", "A black bear blocks the path; wind and a flare offer a harmless solution.", "Cestu blokuje medvěd; vítr a světlice nabízejí neškodné řešení.", Region::forest},
    {36, "firebreak_junction", "FIREBREAK JUNCTION", "KŘIŽOVATKA PRŮSEKŮ", "A turned sign conceals the ravine route until Nell marks it.", "Otočený ukazatel skrývá cestu k rokli, dokud ji Nell neoznačí.", Region::forest},
    {37, "automatic_weather_station", "AUTOMATIC WEATHER STATION", "AUTOMATICKÁ METEOSTANICE", "Stored wind data proves sabotage began six minutes before the storm.", "Uložená data větru dokazují sabotáž šest minut před bouří.", Region::forest},
    {38, "north_fire_lookout", "NORTH FIRE LOOKOUT", "SEVERNÍ POŽÁRNÍ HLÁSKA", "Nell sees Voss's quarry crew and Kestrel Six's weak beacon.", "Nell vidí Vossovu četu v lomu a slabý maják Kestrel Six.", Region::forest, true},

    {39, "ravine_west_lip", "SERVICE RAVINE WEST LIP", "ZÁPADNÍ HRANA SERVISNÍ ROKLE", "An anchor eye can hold the hook and climbing rope.", "Kotevní oko může udržet hák a horolezecké lano.", Region::quarry},
    {40, "broken_service_bridge", "BROKEN SERVICE BRIDGE", "ZŘÍCENÝ SERVISNÍ MOST", "The missing span needs the quarry hoist before it becomes a shortcut.", "Chybějící mostovka potřebuje lomový naviják, aby vznikla zkratka.", Region::quarry},
    {41, "ravine_floor_west", "RAVINE FLOOR WEST", "ZÁPADNÍ DNO ROKLE", "Red survey paint and an old badge wait in the flooded silt.", "Červená průzkumnická barva a starý odznak čekají v naplavenině.", Region::quarry},
    {42, "culvert_mouth", "CULVERT MOUTH", "ÚSTÍ PROPUSTKU", "A magnetic case gleams behind bars in the torch beam.", "Za mříží se v paprsku svítilny leskne magnetické pouzdro.", Region::quarry},
    {43, "waterfall_shelf", "WATERFALL SHELF", "ŘÍMSA ZA VODOPÁDEM", "A small sluice hides the quarry office key in its grate.", "Malé stavidlo ukrývá v mříži klíč od kanceláře lomu.", Region::quarry},
    {44, "ravine_floor_east", "RAVINE FLOOR EAST", "VÝCHODNÍ DNO ROKLE", "Dragged crates climb toward a plaque linking hoist, mine and railway.", "Tažené bedny míří k tabulce spojující naviják, důl a železnici.", Region::quarry},
    {45, "quarry_gate", "QUARRY GATE", "BRÁNA LOMU", "Voss greets the caretaker's apprentice over a field radio.", "Voss osloví správcovu učednici přes polní rádio.", Region::quarry},
    {46, "quarry_office", "QUARRY OFFICE", "KANCELÁŘ LOMU", "Owen is locked in a closet and knows the hoist and crusher horn.", "Owen je zamčený ve skladu a zná naviják i houkačku drtiče.", Region::quarry, true},
    {47, "crusher_deck", "CRUSHER DECK", "PLOŠINA DRTIČE", "Brant patrols between a lethal belt and an inspection cage.", "Brant hlídkuje mezi smrtícím pásem a kontrolní klecí.", Region::quarry},
    {48, "quarry_magazine", "EQUIPMENT MAGAZINE", "SKLAD VYBAVENÍ", "The stolen red phase coil pulses beside Voss's survey notebook.", "Ukradená červená fázová cívka pulzuje vedle Vossova zápisníku.", Region::quarry},
    {49, "quarry_tunnel", "QUARRY TUNNEL", "LOMOVÝ TUNEL", "A broken signal wire denies power to the hoist controls.", "Přerušený signální vodič odpírá napájení ovládání navijáku.", Region::quarry},
    {50, "east_hoist_landing", "EAST HOIST LANDING", "VÝCHODNÍ STANICE NAVIJÁKU", "A repaired hoist deploys the bridge cable and opens the logging route.", "Opravený naviják rozvine mostní kabel a otevře cestu k pile.", Region::quarry},

    {51, "logging_road", "ABANDONED LOGGING ROAD", "OPUŠTĚNÁ LESNÍ CESTA", "Fresh truck tracks end where the road washed away.", "Čerstvé stopy náklaďáku končí tam, kde cestu odnesla voda.", Region::railway},
    {52, "sawmill_yard", "SAWMILL YARD", "DVŮR PILY", "Lila can restore the logging engine when five missing parts are found.", "Lila dokáže obnovit lokomotivu, až se najde pět chybějících částí.", Region::railway, true},
    {53, "sawmill_floor", "SAWMILL FLOOR", "PROVOZ PILY", "An idle planer holds a usable drive belt under tension.", "Odstavená hoblovka drží použitelný hnací řemen pod napětím.", Region::railway},
    {54, "saw_filing_room", "SAW FILING ROOM", "BRUSÍRNA PIL", "An oil can and hand mirror remain beside Calder's grounding notes.", "Olejnička a ruční zrcátko zůstaly vedle Calderiných poznámek o uzemnění.", Region::railway},
    {55, "boiler_house", "BOILER HOUSE", "KOTELNA", "A protected fuel reserve can be siphoned for the engine.", "Chráněnou zásobu paliva lze přečerpat pro lokomotivu.", Region::railway},
    {56, "log_pond", "LOG POND", "KLÁDOVÝ RYBNÍK", "A maintenance box with a spark plug drifts among dangerous logs.", "Mezi nebezpečnými kládami pluje servisní skříňka se svíčkou.", Region::railway},
    {57, "workers_bunkhouse", "WORKERS' BUNKHOUSE", "UBYTOVNA DĚLNÍKŮ", "The rail switch key rests in a foreman's boot.", "Klíč od výhybky leží v předákově botě.", Region::railway},
    {58, "camp_mess_hall", "MESS HALL", "JÍDELNA TÁBORA", "June remembers the whistle and the railway's link to Nightjar.", "June si pamatuje píšťalu a spojení železnice s Nightjarem.", Region::railway},
    {59, "camp_office", "CAMP OFFICE", "KANCELÁŘ TÁBORA", "A mirrored carbon impression reads RIDGE LIFT / 23:40.", "Otisk na kopíráku v zrcadle čte HŘEBENOVÝ VÝTAH / 23:40.", Region::railway},
    {60, "rail_spur_west", "RAIL SPUR WEST", "ZÁPADNÍ KOLEJOVÁ VLEČKA", "The points must align before the engine can leave camp.", "Výhybka se musí srovnat, než lokomotiva opustí tábor.", Region::railway},
    {61, "derelict_logging_engine", "LOGGING ENGINE", "LESNÍ LOKOMOTIVA", "Belt, plug, oil and fuel can wake the old engine.", "Řemen, svíčka, olej a palivo mohou probudit starou lokomotivu.", Region::railway},
    {62, "trestle_approach", "TRESTLE APPROACH", "PŘÍJEZD K VIADUKTU", "A whistle distracts the guard while the brake linkage is repaired.", "Píšťala odláká stráž, zatímco se opraví táhlo brzdy.", Region::railway},
    {63, "east_rail_cut", "EAST RAIL CUT", "VÝCHODNÍ ŽELEZNIČNÍ ZÁŘEZ", "Elias Ward's first intelligible call breaks through the jammer.", "Z rušičky poprvé pronikne srozumitelný hlas Eliase Warda.", Region::railway},

    {64, "dam_overlook", "BLACK PINE DAM OVERLOOK", "VYHLÍDKA NA PŘEHRADU BLACK PINE", "The spillway stands open while Jonah flashes for help.", "Přeliv zůstává otevřený a Jonah bliká o pomoc.", Region::reservoir, true},
    {65, "west_abutment", "WEST ABUTMENT", "ZÁPADNÍ OPĚRA HRÁZE", "Insulated boots and Jonah's dropped badge lie inside a rescue locker.", "V záchranné skříňce leží izolační boty a Jonahův odznak.", Region::reservoir},
    {66, "spillway_walk", "SPILLWAY WALK", "CHODNÍK PŘES PŘELIV", "Timed spray shields guard the route to the gatehouse.", "Časované vodní clony střeží cestu k domku stavidel.", Region::reservoir},
    {67, "gatehouse", "GATEHOUSE", "DOMEK STAVIDEL", "Jonah is trapped behind a false-open spillway command.", "Jonah je uvězněný za falešným povelem k otevření přelivu.", Region::reservoir, true},
    {68, "turbine_hall_upper", "TURBINE HALL UPPER", "HORNÍ TURBÍNOVÁ HALA", "A diagram links dam auxiliary power to the mine substation.", "Schéma spojuje pomocné napájení přehrady s důlní rozvodnou.", Region::reservoir},
    {69, "turbine_hall_lower", "TURBINE HALL LOWER", "DOLNÍ TURBÍNOVÁ HALA", "Three breakers isolate the flooded bay while a pump gasket waits nearby.", "Tři jističe odpojí zatopený prostor a poblíž čeká těsnění čerpadla.", Region::reservoir},
    {70, "pump_gallery", "PUMP GALLERY", "ČERPACÍ GALERIE", "The emergency pump needs a gasket, dry cell and open intake.", "Nouzové čerpadlo potřebuje těsnění, suchý článek a otevřený přívod.", Region::reservoir},
    {71, "flooded_maintenance_bay", "FLOODED MAINTENANCE BAY", "ZATOPENÝ SERVISNÍ PROSTOR", "Drainage reveals a magnet-on-cord in the remaining shallow water.", "Odčerpání odhalí magnet na šňůře ve zbývající mělké vodě.", Region::reservoir},
    {72, "intake_tunnel", "INTAKE TUNNEL", "PŘÍVODNÍ TUNEL", "Kline's chalk message says THE FIELD FOLLOWS THE CARRIER.", "Klineová napsala křídou POLE NÁSLEDUJE NOSNOU VLNU.", Region::reservoir},
    {73, "reservoir_shore", "RESERVOIR SHORE", "BŘEH NÁDRŽE", "Kline's broken glasses lie beside Voss crew bootprints.", "Klineové rozbité brýle leží vedle stop Vossovy čety.", Region::reservoir},
    {74, "valve_garden", "VALVE GARDEN", "VENTILOVÉ POLE", "A missing wheel controls the emergency pump intake.", "Chybějící kolo ovládá přívod nouzového čerpadla.", Region::reservoir},
    {75, "east_access_shaft", "EAST ACCESS SHAFT", "VÝCHODNÍ PŘÍSTUPOVÁ ŠACHTA", "Once the flood falls, Jonah opens the grille into the mine.", "Po poklesu vody Jonah otevře mříž vedoucí do dolu.", Region::reservoir},

    {76, "ore_cart_chamber", "ORE CART CHAMBER", "KOMORA S DŮLNÍM VOZÍKEM", "An emergency cabinet holds the body of a respirator.", "Nouzová skříňka ukrývá tělo respirátoru.", Region::mine, true},
    {77, "timber_gallery", "TIMBER GALLERY", "VÝDŘEVOVÁ CHODBA", "Timber marks identify the brace that must be tightened.", "Značky na výdřevě určují vzpěru, kterou je nutné dotáhnout.", Region::mine},
    {78, "collapsed_drift", "COLLAPSED DRIFT", "ZAVALENÁ CHODBA", "A correctly braced passage survives one controlled rockfall.", "Správně vyztužená chodba přežije jeden řízený sesuv.", Region::mine},
    {79, "ventilation_room", "VENTILATION ROOM", "VĚTRACÍ STROJOVNA", "Charcoal completes the respirator while the multimeter restores the fan.", "Uhlí dokončí respirátor a multimetr obnoví ventilátor.", Region::mine},
    {80, "copper_vein", "COPPER VEIN", "MĚDĚNÁ ŽÍLA", "Gas surrounds a cut copper bus bar left by a survey drill.", "Plyn obklopuje měděnou přípojnici opuštěnou u vrtáku.", Region::mine},
    {81, "mine_pump_station", "MINE PUMP STATION", "DŮLNÍ ČERPACÍ STANICE", "Restored drainage weakens the current in the flooded drift.", "Obnovené odvodnění oslabí proud v zatopené chodbě.", Region::mine},
    {82, "flooded_drift", "FLOODED DRIFT", "ZATOPENÁ CHODBA", "A lift fuse waits beneath a submerged grate for the magnet.", "Pojistka výtahu čeká pod ponořenou mříží na magnet.", Region::mine},
    {83, "survey_chamber", "SURVEY CHAMBER", "PRŮZKUMNÁ KOMORA", "Voss abandoned Kline's badge, mine map and punched emergency card.", "Voss opustil Klineové odznak, důlní mapu a děrný nouzový štítek.", Region::mine},
    {84, "freight_lift_bottom", "FREIGHT LIFT BOTTOM", "DOLNÍ STANICE NÁKLADNÍHO VÝTAHU", "A new fuse cannot move the cage until the substation is repaired.", "Nová pojistka nepohne klecí, dokud nebude opravena rozvodna.", Region::mine},
    {85, "freight_lift_top", "FREIGHT LIFT TOP", "HORNÍ STANICE NÁKLADNÍHO VÝTAHU", "Kade's radio carries Voss's order to flood the shaft.", "Kadeovo rádio přenáší Vossův rozkaz zatopit šachtu.", Region::mine},
    {86, "underground_substation", "UNDERGROUND SUBSTATION", "PODZEMNÍ ROZVODNA", "The routing board must feed the lift, never the Quiet Field trunk.", "Rozvodná deska musí napájet výtah, nikdy trasu Tichého pole.", Region::mine, true},
    {87, "switchgear_aisle", "SWITCHGEAR AISLE", "ULIČKA ROZVADĚČŮ", "Calder's scratched arrows reveal a safe breaker order.", "Calderiny vyškrábané šipky ukazují bezpečné pořadí jističů.", Region::mine},
    {88, "cable_vault", "CABLE VAULT", "KABELOVÁ KOMORA", "The black Quiet Field feed can be replaced by the copper lift bus.", "Černý přívod Tichého pole lze nahradit měděnou přípojnicí výtahu.", Region::mine},
    {89, "sealed_research_door", "SEALED RESEARCH DOOR", "UTĚSNĚNÉ VÝZKUMNÉ DVEŘE", "An inverted punched card reveals Kline's emergency access.", "Obrácený děrný štítek odhalí Klineové nouzový přístup.", Region::mine},
    {90, "ridge_freight_lift", "RIDGE FREIGHT LIFT", "HŘEBENOVÝ NÁKLADNÍ VÝTAH", "Voss offers safe passage if Iris abandons the stolen phase coil.", "Voss nabídne bezpečný průchod, pokud Iris opustí ukradenou fázovou cívku.", Region::mine},

    {91, "freight_lift_lobby", "RIDGE LIFT LOBBY", "VESTIBUL HŘEBENOVÉHO VÝTAHU", "A tracking camera can be blinded with the hand mirror.", "Sledující kameru lze oslepit ručním zrcátkem.", Region::observatory},
    {92, "ridge_courtyard", "RIDGE COURTYARD", "HŘEBENOVÉ NÁDVOŘÍ", "Kade and Morrow patrol between searchlights and locked laboratories.", "Kade a Morrow hlídkují mezi světlomety a zamčenými laboratořemi.", Region::observatory, true},
    {93, "observatory_dormitory", "OBSERVATORY DORMITORY", "UBYTOVNA OBSERVATOŘE", "Voss's bunk notes name a midnight buyer call.", "Poznámky u Vossova lůžka zmiňují půlnoční hovor s kupcem.", Region::observatory},
    {94, "observatory_kitchen", "OBSERVATORY KITCHEN", "KUCHYNĚ OBSERVATOŘE", "A timer and sealed ration can draw one guard away.", "Minutka a uzavřená dávka mohou odlákat jednoho strážného.", Region::observatory},
    {95, "observatory_infirmary", "OBSERVATORY INFIRMARY", "OŠETŘOVNA OBSERVATOŘE", "Kline left a recording for whoever restores the carrier.", "Klineová nechala nahrávku tomu, kdo obnoví nosnou vlnu.", Region::observatory},
    {96, "archive_hall", "ARCHIVE HALL", "ARCHIVNÍ HALA", "Project portraits establish the conflict between Calder and Voss.", "Projektové portréty objasňují spor mezi Calderovou a Vossem.", Region::observatory},
    {97, "records_room", "RECORDS ROOM", "SPISOVNA", "Four project dates unlock a cipher lens and magnetic archive reel.", "Čtyři data projektu odemknou šifrovací čočku a magnetický archivní kotouč.", Region::observatory},
    {98, "weather_lab", "WEATHER LAB", "METEOROLOGICKÁ LABORATOŘ", "Natural storm data surrounds Voss's stolen phase prism.", "Údaje o přirozené bouři obklopují Vossův ukradený fázový hranol.", Region::observatory},
    {99, "instrument_dome", "INSTRUMENT DOME", "PŘÍSTROJOVÁ KOPULE", "The north-aligned dome powers the archive reader and hides a tuning fork.", "Kopule srovnaná na sever napájí čtečku archivu a ukrývá ladičku.", Region::observatory},
    {100, "telescope_platform", "TELESCOPE PLATFORM", "TELESKOPOVÁ PLOŠINA", "Nell's landmarks confirm tower azimuth and wake the fog horn.", "Nelliny orientační body potvrdí azimut věže a probudí mlhovou sirénu.", Region::observatory},
    {101, "communications_lab", "COMMUNICATIONS LAB", "KOMUNIKAČNÍ LABORATOŘ", "Calder's warning and Theo's evidence can turn Sable against Voss.", "Calderové varování a Theovy důkazy mohou obrátit Sable proti Vossovi.", Region::observatory, true},
    {102, "security_office", "SECURITY OFFICE", "BEZPEČNOSTNÍ KANCELÁŘ", "A mirrored keypad protects the instrument-dome key.", "Klávesnice čitelná v zrcadle chrání klíč od přístrojové kopule.", Region::observatory},
    {103, "nightjar_antechamber", "NIGHTJAR ANTECHAMBER", "PŘEDSÍŇ NIGHTJARU", "Badge, Calder phrase and calibration tone open three bunker locks.", "Odznak, Calderové fráze a kalibrační tón otevřou tři zámky bunkru.", Region::observatory},

    {104, "decontamination_hall", "DECONTAMINATION HALL", "DEKONTAMINAČNÍ HALA", "A harmless air cycle now serves as Nightjar security.", "Neškodný vzduchový cyklus nyní slouží jako zabezpečení Nightjaru.", Region::bunker, true},
    {105, "main_bunker_corridor", "MAIN BUNKER CORRIDOR", "HLAVNÍ CHODBA BUNKRU", "Calder's recorded voice can lure the remaining guards into decontamination.", "Calderové nahraný hlas může nalákat zbývající stráže do dekontaminace.", Region::bunker},
    {106, "phase_lab", "PHASE LABORATORY", "FÁZOVÁ LABORATOŘ", "Coil and prism in the diagnostic rig calculate safe inversion values.", "Cívka a hranol v diagnostice vypočítají bezpečné hodnoty inverze.", Region::bunker},
    {107, "calibration_chamber", "CALIBRATION CHAMBER", "KALIBRAČNÍ KOMORA", "The cipher colours order three fork positions into sequence 4-1-3.", "Barvy šifry seřadí tři polohy ladičky do sekvence 4-1-3.", Region::bunker},
    {108, "quiet_field_test_cell", "QUIET FIELD TEST CELL", "ZKUŠEBNÍ KOMORA TICHÉHO POLE", "Calder explains the silent bell while Kline signals through glass.", "Calderová vysvětluje němý zvon a Klineová signalizuje přes sklo.", Region::bunker},
    {109, "bunker_machine_shop", "MACHINE SHOP", "STROJNÍ DÍLNA", "A seized rack holds a coolant hose and grounding clamp.", "Zadřený stojan drží chladicí hadici a zemnicí svorku.", Region::bunker},
    {110, "capacitor_hall", "CAPACITOR HALL", "KONDENZÁTOROVÁ HALA", "Three charged banks must be grounded in protected order 4-1-3.", "Tři nabité bloky je nutné uzemnit v chráněném pořadí 4-1-3.", Region::bunker},
    {111, "cooling_gallery", "COOLING GALLERY", "CHLADICÍ GALERIE", "A replacement hose diverts cooling into the emergency dump.", "Náhradní hadice odvede chlazení do nouzové výpusti.", Region::bunker},
    {112, "command_archive", "COMMAND ARCHIVE", "VELITELSKÝ ARCHIV", "The complete evidence spool records Voss admitting the demonstration.", "Úplný důkazní kotouč zaznamená Vossovo přiznání k demonstraci.", Region::bunker},
    {113, "holding_room", "HOLDING ROOM", "ZADRŽOVACÍ MÍSTNOST", "Miriam Kline knows the inversion plan and carries an override key.", "Miriam Klineová zná plán inverze a má nouzový klíč.", Region::bunker, true},
    {114, "emergency_stair", "EMERGENCY STAIR", "NOUZOVÉ SCHODIŠTĚ", "Voss cuts the lights and retreats while Sable holds the lower door.", "Voss zhasne a ustoupí, zatímco Sable drží spodní dveře.", Region::bunker},
    {115, "summit_access_lock", "SUMMIT ACCESS LOCK", "ZÁMEK PŘÍSTUPU NA VRCHOL", "The override reports eighteen minutes until the final field pulse.", "Nouzové ovládání hlásí osmnáct minut do posledního pulzu pole.", Region::bunker},

    {116, "storm_stair_lower", "STORM STAIR LOWER", "DOLNÍ BOUŘKOVÉ SCHODIŠTĚ", "A broken grounding cable makes every metal step suspect.", "Přerušené uzemnění činí každý kovový schod podezřelým.", Region::summit},
    {117, "windbreak_ledge", "WINDBREAK LEDGE", "ŘÍMSA VE VĚTRNÉM STÍNU", "Stone shelters and a fixed handline break the wind and falling rock.", "Kamenné kryty a pevné lano tlumí vítr i padající kamení.", Region::summit},
    {118, "lightning_gallery", "LIGHTNING GALLERY", "BLESKOVÁ GALERIE", "A clamp can bridge the broken copper strap and ground the summit.", "Svorka může překlenout prasklý měděný pás a uzemnit vrchol.", Region::summit},
    {119, "tower_base", "BLACK PINE TOWER BASE", "PATA VĚŽE BLACK PINE", "The recovered phase coil belongs in the tower feed cabinet.", "Získaná fázová cívka patří do napájecí skříně věže.", Region::summit, true},
    {120, "mid_tower_platform", "MID-TOWER PLATFORM", "STŘEDNÍ PLOŠINA VĚŽE", "Mara and Elias break through as the jammer weakens.", "Mara a Elias proniknou éterem, když rušení slábne.", Region::summit},
    {121, "microwave_deck", "MICROWAVE DECK", "MIKROVLNNÁ PLOŠINA", "The phase prism and calibration fork can tune the waveguide.", "Fázový hranol a ladička mohou naladit vlnovod.", Region::summit},
    {122, "beacon_ring", "BEACON RING", "PRSTENEC MAJÁKU", "A cleaned beacon crystal becomes the protected-carrier reference.", "Očištěný krystal majáku se stane referencí chráněné nosné vlny.", Region::summit},
    {123, "antenna_service_platform", "ANTENNA SERVICE PLATFORM", "SERVISNÍ PLOŠINA ANTÉNY", "The alignment chart, wrench and override key defeat Voss's motor control.", "Seřizovací plán, klíč a nouzový klíč porazí Vossovo ovládání motoru.", Region::summit},
    {124, "summit_control_capsule", "SUMMIT CONTROL CAPSULE", "VRCHOLOVÁ ŘÍDICÍ KABINA", "The protected carrier can collapse the Quiet Field and carry every voice home.", "Chráněná nosná vlna může zhroutit Tiché pole a vrátit domů všechny hlasy.", Region::summit},
}};

} // namespace black_pine::content
