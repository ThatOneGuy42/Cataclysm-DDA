#include "player.h"
#include "game.h"
#include "rng.h"
#include "input.h"
#include "keypress.h"
#include "item.h"
#include "bionics.h"
#include "line.h"
#include "catajson.h"

#define BATTERY_AMOUNT 4 // How much batteries increase your power

std::map<bionic_id, bionic_data*> bionics;
std::vector<bionic_id> faulty_bionics;
std::vector<bionic_id> power_source_bionics;
std::vector<bionic_id> unpowered_bionics;

void bionics_install_failure(game *g, player *u, int success);

bionic_data::bionic_data(std::string new_name, bool new_power_source, bool new_activated,
                          int new_power_cost, int new_charge_time, std::string new_description, bool new_faulty){
   name = new_name;
   power_source = new_power_source;
   activated = new_activated;
   power_cost = new_power_cost;
   charge_time = new_charge_time;
   description = new_description;
   faulty = new_faulty;
}

bionic_id game::random_good_bionic() const
{
    std::map<std::string,bionic_data*>::const_iterator random_bionic;
    do
    {
        random_bionic = bionics.begin();
        std::advance(random_bionic,rng(0,bionics.size()-1));
    } while (random_bionic->first == "bio_null" || random_bionic->second->faulty);
    return random_bionic->first;
}

// helper function for power_bionics
void show_power_level_in_titlebar(WINDOW* window, player* p)
{
    mvwprintz(window, 1, 62, c_white, "Power: %d/%d", p->power_level, p->max_power_level);
}

void player::power_bionics(game *g)
{
    int HEIGHT = TERMY;
    int WIDTH = FULL_SCREEN_WIDTH;
    int START_X = (TERMX - WIDTH)/2;
    int START_Y = (TERMY - HEIGHT)/2;
 WINDOW* wBio = newwin(HEIGHT, WIDTH, START_Y, START_X);
    int DESCRIPTION_WIDTH = WIDTH - 2; // Same width as bionics window minus 2 for the borders
    int DESCRIPTION_HEIGHT = 3;
    int DESCRIPTION_START_X = getbegx(wBio) + 1; // +1 to avoid border
    int DESCRIPTION_START_Y = getmaxy(wBio) - DESCRIPTION_HEIGHT - 1; // At the bottom of the bio window, -1 to avoid border
 WINDOW* w_description = newwin(DESCRIPTION_HEIGHT, DESCRIPTION_WIDTH, DESCRIPTION_START_Y, DESCRIPTION_START_X);

 werase(wBio);
 wborder(wBio, LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
               LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX );

 std::vector <bionic> passive;
 std::vector <bionic> active;
 int HEADER_TEXT_Y = START_Y + 1;
 mvwprintz(wBio, HEADER_TEXT_Y,  1, c_blue, "BIONICS -");
 mvwprintz(wBio, HEADER_TEXT_Y, 11, c_white, "Activating.  Press '!' to examine your implants.");
 show_power_level_in_titlebar(wBio, this);

 int HEADER_LINE_Y = START_Y + 2;
 int DESCRIPTION_LINE_Y = DESCRIPTION_START_Y - 1;
 for (int i = 1; i < 79; i++) {
  mvwputch(wBio, HEADER_LINE_Y, i, c_ltgray, LINE_OXOX); // Draw line under title
  mvwputch(wBio, DESCRIPTION_LINE_Y, i, c_ltgray, LINE_OXOX); // Draw line above description
 }

 // Draw symbols to connect additional lines to border
 mvwputch(wBio, HEADER_LINE_Y,  0, c_ltgray, LINE_XXXO); // |-
 mvwputch(wBio, HEADER_LINE_Y, 79, c_ltgray, LINE_XOXX); // -|

 mvwputch(wBio, DESCRIPTION_LINE_Y,  0, c_ltgray, LINE_XXXO); // |-
 mvwputch(wBio, DESCRIPTION_LINE_Y, 79, c_ltgray, LINE_XOXX); // -|

 for (int i = 0; i < my_bionics.size(); i++) {
  if (!bionics[my_bionics[i].id]->activated)
   passive.push_back(my_bionics[i]);
  else
   active.push_back(my_bionics[i]);
 }
 nc_color type;
 if (passive.size() > 0) {
  mvwprintz(wBio, 3, 1, c_ltblue, "Passive:");
  for (int i = 0; i < passive.size(); i++) {
   if (bionics[passive[i].id]->power_source)
    type = c_ltcyan;
   else
    type = c_cyan;
   mvwputch(wBio, 4 + i, 1, type, passive[i].invlet);
   mvwprintz(wBio, 4 + i, 3, type, bionics[passive[i].id]->name.c_str());
  }
 }
 if (active.size() > 0) {
  mvwprintz(wBio, 3, 33, c_ltblue, "Active:");
  for (int i = 0; i < active.size(); i++) {
   if (active[i].powered && !bionics[active[i].id]->power_source)
    type = c_red;
    else if (bionics[active[i].id]->power_source && !active[i].powered)
    type = c_ltcyan;
    else if (bionics[active[i].id]->power_source && active[i].powered)
    type = c_ltgreen;
   else
    type = c_ltred;

   mvwputch(wBio, 4 + i, 33, type, active[i].invlet);
   mvwprintz(wBio, 4 + i, 35, type,
             (active[i].powered ? "%s - ON" : "%s - %d PU / %d trns"),
             bionics[active[i].id]->name.c_str(),
             bionics[active[i].id]->power_cost,
             bionics[active[i].id]->charge_time);
  }
 }
 wrefresh(wBio);
 char ch;
 bool activating = true;
 bionic *tmp;
 int b;
 do {
  ch = getch();
  if (ch == '!') {
   activating = !activating;
   if (activating)
    mvwprintz(wBio, 1, 11, c_white, "Activating.  Press '!' to examine your implants.");
   else
    mvwprintz(wBio, 1, 11, c_white, "Examining.  Press '!' to activate your implants.");
  } else if (ch == ' ')
   ch = KEY_ESCAPE;
  else if (ch != KEY_ESCAPE) {
   for (int i = 0; i < my_bionics.size(); i++) {
    if (ch == my_bionics[i].invlet) {
     tmp = &my_bionics[i];
     b = i;
     ch = KEY_ESCAPE;
    }
   }
   if (ch == KEY_ESCAPE) {
    if (activating) {
     if (bionics[tmp->id]->activated) {
      show_power_level_in_titlebar(wBio, this);
      itype_id weapon_id = weapon.type->id;
      if (tmp->powered) {
       tmp->powered = false;
       g->add_msg("%s powered off.", bionics[tmp->id]->name.c_str());
      } else if (power_level >= bionics[tmp->id]->power_cost ||
                 (weapon_id == "bio_claws_weapon" && tmp->id == "bio_claws_weapon"))
       activate_bionic(b, g);
     } else
      mvwprintz(wBio, 21, 1, c_ltred, "\
You can not activate %s!  To read a description of \
%s, press '!', then '%c'.", bionics[tmp->id]->name.c_str(),
                            bionics[tmp->id]->name.c_str(), tmp->invlet);
    } else {	// Describing bionics, not activating them!
// Clear the lines first
     ch = 0;
     werase(w_description);
     mvwprintz(w_description, 0, 0, c_ltblue, bionics[tmp->id]->description.c_str());
    }
   }
  }
  wrefresh(w_description);
  wrefresh(wBio);
 } while (ch != KEY_ESCAPE);
 werase(wBio);
 wrefresh(wBio);
 delwin(wBio);
 erase();
}

// Why put this in a Big Switch?  Why not let bionics have pointers to
// functions, much like monsters and items?
//
// Well, because like diseases, which are also in a Big Switch, bionics don't
// share functions....
void player::activate_bionic(int b, game *g)
{
 bionic bio = my_bionics[b];
 int power_cost = bionics[bio.id]->power_cost;
 if (weapon.type->id == "bio_claws_weapon" && bio.id == "bio_claws_weapon")
  power_cost = 0;
 if (power_level < power_cost) {
  if (my_bionics[b].powered) {
   g->add_msg("Your %s powers down.", bionics[bio.id]->name.c_str());
   my_bionics[b].powered = false;
  } else
   g->add_msg("You cannot power your %s", bionics[bio.id]->name.c_str());
  return;
 }

 if (my_bionics[b].powered && my_bionics[b].charge > 0) {
// Already-on units just lose a bit of charge
  my_bionics[b].charge--;
 } else {
// Not-on units, or those with zero charge, have to pay the power cost
  if (bionics[bio.id]->charge_time > 0) {
   my_bionics[b].powered = true;
   my_bionics[b].charge = bionics[bio.id]->charge_time;
  }
  power_level -= power_cost;
 }

 std::string junk;
 std::vector<point> traj;
 std::vector<std::string> good;
 std::vector<std::string> bad;
 WINDOW* w;
 int dirx, diry, t, l, index;
 InputEvent input;
 item tmp_item;

 if(bio.id == "bio_painkiller"){
  pkill += 6;
  pain -= 2;
  if (pkill > pain)
   pkill = pain;
 } else if (bio.id == "bio_nanobots"){
  healall(4);
 } else if (bio.id == "bio_night"){
  if (g->turn % 5)
    g->add_msg("Artificial night generator active!");
 } else if (bio.id == "bio_resonator"){
  g->sound(posx, posy, 30, "VRRRRMP!");
  for (int i = posx - 1; i <= posx + 1; i++) {
   for (int j = posy - 1; j <= posy + 1; j++) {
    g->m.bash(i, j, 40, junk);
    g->m.bash(i, j, 40, junk);	// Multibash effect, so that doors &c will fall
    g->m.bash(i, j, 40, junk);
    if (g->m.is_destructable(i, j) && rng(1, 10) >= 4)
     g->m.ter_set(i, j, t_rubble);
   }
  }
 } else if (bio.id == "bio_time_freeze"){
  moves += 100 * power_level;
  power_level = 0;
  g->add_msg("Your speed suddenly increases!");
  if (one_in(3)) {
   g->add_msg("Your muscles tear with the strain.");
   hurt(g, bp_arms, 0, rng(5, 10));
   hurt(g, bp_arms, 1, rng(5, 10));
   hurt(g, bp_legs, 0, rng(7, 12));
   hurt(g, bp_legs, 1, rng(7, 12));
   hurt(g, bp_torso, 0, rng(5, 15));
  }
  if (one_in(5))
   add_disease(DI_TELEGLOW, rng(50, 400), g);
 } else if (bio.id == "bio_teleport"){
  g->teleport();
  add_disease(DI_TELEGLOW, 300, g);
 }
// TODO: More stuff here (and bio_blood_filter)
 else if(bio.id == "bio_blood_anal"){
  w = newwin(20, 40, 3 + ((TERMY > 25) ? (TERMY-25)/2 : 0), 10+((TERMX > 80) ? (TERMX-80)/2 : 0));
  wborder(w, LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
             LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX );
  if (has_disease(DI_FUNGUS))
   bad.push_back("Fungal Parasite");
  if (has_disease(DI_DERMATIK))
   bad.push_back("Insect Parasite");
  if (has_disease(DI_POISON))
   bad.push_back("Poison");
  if (radiation > 0)
   bad.push_back("Irradiated");
  if (has_disease(DI_PKILL1))
   good.push_back("Minor Painkiller");
  if (has_disease(DI_PKILL2))
   good.push_back("Moderate Painkiller");
  if (has_disease(DI_PKILL3))
   good.push_back("Heavy Painkiller");
  if (has_disease(DI_PKILL_L))
   good.push_back("Slow-Release Painkiller");
  if (has_disease(DI_DRUNK))
   good.push_back("Alcohol");
  if (has_disease(DI_CIG))
   good.push_back("Nicotine");
  if (has_disease(DI_METH))
    good.push_back("Methamphetamines");
  if (has_disease(DI_HIGH))
   good.push_back("Intoxicant: Other");
  if (has_disease(DI_HALLU) || has_disease(DI_VISUALS))
   bad.push_back("Magic Mushroom");
  if (has_disease(DI_IODINE))
   good.push_back("Iodine");
  if (has_disease(DI_TOOK_XANAX))
   good.push_back("Xanax");
  if (has_disease(DI_TOOK_PROZAC))
   good.push_back("Prozac");
  if (has_disease(DI_TOOK_FLUMED))
   good.push_back("Antihistamines");
  if (has_disease(DI_ADRENALINE))
   good.push_back("Adrenaline Spike");
  if (good.size() == 0 && bad.size() == 0)
   mvwprintz(w, 1, 1, c_white, "No effects.");
  else {
   for (int line = 1; line < 39 && line <= good.size() + bad.size(); line++) {
    if (line <= bad.size())
     mvwprintz(w, line, 1, c_red, bad[line - 1].c_str());
    else
     mvwprintz(w, line, 1, c_green, good[line - 1 - bad.size()].c_str());
   }
  }
  wrefresh(w);
  refresh();
  getch();
  delwin(w);
 } else if(bio.id == "bio_blood_filter"){
  rem_disease(DI_FUNGUS);
  rem_disease(DI_POISON);
  rem_disease(DI_PKILL1);
  rem_disease(DI_PKILL2);
  rem_disease(DI_PKILL3);
  rem_disease(DI_PKILL_L);
  rem_disease(DI_DRUNK);
  rem_disease(DI_CIG);
  rem_disease(DI_HIGH);
  rem_disease(DI_HALLU);
  rem_disease(DI_VISUALS);
  rem_disease(DI_IODINE);
  rem_disease(DI_TOOK_XANAX);
  rem_disease(DI_TOOK_PROZAC);
  rem_disease(DI_TOOK_FLUMED);
  rem_disease(DI_ADRENALINE);
  rem_disease(DI_METH);
  pkill = 0;
  stim = 0;
 } else if(bio.id == "bio_evap"){
  if (query_yn("Drink directly? Otherwise you will need a container.")) {
   tmp_item = item(g->itypes["water_clean"], 0);
   thirst -= 50;
   if (has_trait(PF_GOURMAND) && thirst < -60) {
     g->add_msg("You can't finish it all!");
     thirst = -60;
   } else if (!has_trait(PF_GOURMAND) && thirst < -20) {
     g->add_msg("You can't finish it all!");
     thirst = -20;
   }
  } else {
   t = g->inv("Choose a container:");
   if (i_at(t).type == 0) {
    g->add_msg("You don't have that item!");
    power_level += bionics["bio_evap"]->power_cost;
   } else if (!i_at(t).is_container()) {
    g->add_msg("That %s isn't a container!", i_at(t).tname().c_str());
    power_level += bionics["bio_evap"]->power_cost;
   } else {
    it_container *cont = dynamic_cast<it_container*>(i_at(t).type);
    if (i_at(t).volume_contained() + 1 > cont->contains) {
     g->add_msg("There's no space left in your %s.", i_at(t).tname().c_str());
     power_level += bionics["bio_evap"]->power_cost;
    } else if (!(cont->flags & con_wtight)) {
     g->add_msg("Your %s isn't watertight!", i_at(t).tname().c_str());
     power_level += bionics["bio_evap"]->power_cost;
    } else {
     g->add_msg("You pour water into your %s.", i_at(t).tname().c_str());
     i_at(t).put_in(item(g->itypes["water_clean"], 0));
    }
   }
  }
 } else if(bio.id == "bio_lighter"){
  g->draw();
  mvprintw(0, 0, "Torch in which direction?");
  input = get_input();
  get_direction(dirx, diry, input);
  if (dirx == -2) {
   g->add_msg("Invalid direction.");
   power_level += bionics["bio_lighter"]->power_cost;
   return;
  }
  dirx += posx;
  diry += posy;
  if (!g->m.add_field(g, dirx, diry, fd_fire, 1))	// Unsuccessful.
   g->add_msg("You can't light a fire there.");
 } else if(bio.id == "bio_claws"){
  if (weapon.type->id == "bio_claws_weapon") {
   g->add_msg("You withdraw your claws.");
   weapon = get_combat_style();
  } else if(weapon.type->id != "null"){
   g->add_msg("Your claws extend, forcing you to drop your %s.",
              weapon.tname().c_str());
   g->m.add_item(posx, posy, weapon);
   weapon = item(g->itypes["bio_claws_weapon"], 0);
   weapon.invlet = '#';
  } else {
   g->add_msg("Your claws extend!");
   weapon = item(g->itypes["bio_claws_weapon"], 0);
   weapon.invlet = '#';
  }
 } else if(bio.id == "bio_blaster"){
  tmp_item = weapon;
  weapon = item(g->itypes["bio_blaster_gun"], 0);
  weapon.curammo = dynamic_cast<it_ammo*>(g->itypes["bio_fusion_ammo"]);
  weapon.charges = 1;
  g->refresh_all();
  g->plfire(false);
  weapon = tmp_item;
 } else if (bio.id == "bio_laser"){
  tmp_item = weapon;
  weapon = item(g->itypes["v29"], 0);
  weapon.curammo = dynamic_cast<it_ammo*>(g->itypes["laser_pack"]);
  weapon.charges = 1;
  g->refresh_all();
  g->plfire(false);
  weapon = tmp_item;
 } else if (bio.id == "bio_emp"){
  g->draw();
  mvprintw(0, 0, "Fire EMP in which direction?");
  input = get_input();
  get_direction(dirx, diry, input);
  if (dirx == -2) {
   g->add_msg("Invalid direction.");
   power_level += bionics["bio_emp"]->power_cost;
   return;
  }
  dirx += posx;
  diry += posy;
  g->emp_blast(dirx, diry);
 } else if (bio.id == "bio_hydraulics"){
  g->add_msg("Your muscles hiss as hydraulic strength fills them!");
 } else if (bio.id == "bio_water_extractor"){
  for (int i = 0; i < g->m.i_at(posx, posy).size(); i++) {
   item tmp = g->m.i_at(posx, posy)[i];
   if (tmp.type->id == "corpse" && query_yn("Extract water from the %s",
                                              tmp.tname().c_str())) {
    i = g->m.i_at(posx, posy).size() + 1;	// Loop is finished
    t = g->inv("Choose a container:");
    if (i_at(t).type == 0) {
     g->add_msg("You don't have that item!");
     power_level += bionics["bio_water_extractor"]->power_cost;
    } else if (!i_at(t).is_container()) {
     g->add_msg("That %s isn't a container!", i_at(t).tname().c_str());
     power_level += bionics["bio_water_extractor"]->power_cost;
    } else {
     it_container *cont = dynamic_cast<it_container*>(i_at(t).type);
     if (i_at(t).volume_contained() + 1 > cont->contains) {
      g->add_msg("There's no space left in your %s.", i_at(t).tname().c_str());
      power_level += bionics["bio_water_extractor"]->power_cost;
     } else {
      g->add_msg("You pour water into your %s.", i_at(t).tname().c_str());
      i_at(t).put_in(item(g->itypes["water"], 0));
     }
    }
   }
   if (i == g->m.i_at(posx, posy).size() - 1)	// We never chose a corpse
    power_level += bionics["bio_water_extractor"]->power_cost;
  }
 } else if(bio.id == "bio_magnet"){
  for (int i = posx - 10; i <= posx + 10; i++) {
   for (int j = posy - 10; j <= posy + 10; j++) {
    if (g->m.i_at(i, j).size() > 0) {
     if (g->m.sees(i, j, posx, posy, -1, t))
      traj = line_to(i, j, posx, posy, t);
     else
      traj = line_to(i, j, posx, posy, 0);
    }
    traj.insert(traj.begin(), point(i, j));
    for (int k = 0; k < g->m.i_at(i, j).size(); k++) {
     if (g->m.i_at(i, j)[k].made_of("iron") || g->m.i_at(i, j)[k].made_of("steel")){
      tmp_item = g->m.i_at(i, j)[k];
      g->m.i_rem(i, j, k);
      for (l = 0; l < traj.size(); l++) {
       index = g->mon_at(traj[l].x, traj[l].y);
       if (index != -1) {
        if (g->z[index].hurt(tmp_item.weight() * 2))
         g->kill_mon(index, true);
        g->m.add_item(traj[l].x, traj[l].y, tmp_item);
        l = traj.size() + 1;
       } else if (l > 0 && g->m.move_cost(traj[l].x, traj[l].y) == 0) {
        g->m.bash(traj[l].x, traj[l].y, tmp_item.weight() * 2, junk);
        g->sound(traj[l].x, traj[l].y, 12, junk);
        if (g->m.move_cost(traj[l].x, traj[l].y) == 0) {
         g->m.add_item(traj[l - 1].x, traj[l - 1].y, tmp_item);
         l = traj.size() + 1;
        }
       }
      }
      if (l == traj.size())
       g->m.add_item(posx, posy, tmp_item);
     }
    }
   }
  }
 } else if(bio.id == "bio_lockpick"){
  g->draw();
  mvprintw(0, 0, "Unlock in which direction?");
  input = get_input();
  get_direction(dirx, diry, input);
  if (dirx == -2) {
   g->add_msg("Invalid direction.");
   power_level += bionics["bio_lockpick"]->power_cost;
   return;
  }
  dirx += posx;
  diry += posy;
  if (g->m.ter(dirx, diry) == t_door_locked) {
   moves -= 40;
   g->add_msg("You unlock the door.");
   g->m.ter_set(dirx, diry, t_door_c);
  } else
   g->add_msg("You can't unlock that %s.", g->m.tername(dirx, diry).c_str());
 } else if(bio.id == "bio_flashbang") {
   g->add_msg("You activate your integrated flashbang generator!");
   g->flashbang(posx, posy, true);
 } else if(bio.id == "bio_shockwave") {
   g->shockwave(posx, posy, 3, 4, 2, 8, true);
   g->add_msg("You unleash a powerful shockwave!");
 } else if(bio.id == "bio_chain_lightning"){
  tmp_item = weapon;
  weapon = item(g->itypes["bio_lightning"], 0);
  weapon.curammo = dynamic_cast<it_ammo*>(g->itypes["bio_lightning_ammo"]);
  weapon.charges = 10;
  g->refresh_all();
  g->plfire(false);
  weapon = tmp_item;
 }
}

bool player::install_bionics(game *g, it_bionic* type)
{
 if (type == NULL) {
  debugmsg("Tried to install NULL bionic");
  return false;
 }
 std::string bio_name = type->name.substr(5);	// Strip off "CBM: "

 WINDOW* w = newwin(FULL_SCREEN_HEIGHT, FULL_SCREEN_WIDTH, (TERMY > FULL_SCREEN_HEIGHT) ? (TERMY-FULL_SCREEN_HEIGHT)/2 : 0, (TERMX > FULL_SCREEN_WIDTH) ? (TERMX-FULL_SCREEN_WIDTH)/2 : 0);
 WINDOW* w_description = newwin(3, FULL_SCREEN_WIDTH-2, 21 + getbegy(w), 1 + getbegx(w));

 werase(w);
 wborder(w, LINE_XOXO, LINE_XOXO, LINE_OXOX, LINE_OXOX,
            LINE_OXXO, LINE_OOXX, LINE_XXOO, LINE_XOOX );

 int pl_skill = int_cur +
   skillLevel("electronics") * 4 +
   skillLevel("firstaid")    * 3 +
   skillLevel("mechanics")   * 2;

 int skint = int(pl_skill / 4);
 int skdec = int((pl_skill * 10) / 4) % 10;

// Header text
 mvwprintz(w, 1,  1, c_white, "Installing bionics:");
 mvwprintz(w, 1, 21, type->color, bio_name.c_str());

// Dividing bars
 for (int i = 1; i < 79; i++) {
  mvwputch(w,  2, i, c_ltgray, LINE_OXOX);
  mvwputch(w, 20, i, c_ltgray, LINE_OXOX);
 }

 mvwputch(w, 2,  0, c_ltgray, LINE_XXXO); // |-
 mvwputch(w, 2, 79, c_ltgray, LINE_XOXX); // -|

 mvwputch(w, 20,  0, c_ltgray, LINE_XXXO); // |-
 mvwputch(w, 20, 79, c_ltgray, LINE_XOXX); // -|

// Init the list of bionics
 for (int i = 1; i < type->options.size(); i++) {
  bionic_id bio_id = type->options[i];
  mvwprintz(w, i + 3, 1, (has_bionic(bio_id) ? c_ltred : c_ltblue),
            bionics[bio_id]->name.c_str());
 }
// Helper text
 mvwprintz(w, 3, 39, c_white,        "Difficulty of this module: %d",
           type->difficulty);
 mvwprintz(w, 4, 39, c_white,        "Your installation skill:   %d.%d",
           skint, skdec);
 mvwprintz(w, 5, 39, c_white,       "Installation requires high intelligence,");
 mvwprintz(w, 6, 39, c_white,       "and skill in electronics, first aid, and");
 mvwprintz(w, 7, 39, c_white,       "mechanics (in that order of importance).");

 int chance_of_success = int((100 * pl_skill) /
                             (pl_skill + 4 * type->difficulty));

 mvwprintz(w, 9, 39, c_white,        "Chance of success:");

 nc_color col_suc;
 if (chance_of_success >= 95)
  col_suc = c_green;
 else if (chance_of_success >= 80)
  col_suc = c_ltgreen;
 else if (chance_of_success >= 60)
  col_suc = c_yellow;
 else if (chance_of_success >= 35)
  col_suc = c_ltred;
 else
  col_suc = c_red;

 mvwprintz(w, 9, 59, col_suc, "%d%%%%", chance_of_success);

 mvwprintz(w, 11, 39, c_white,       "Failure may result in crippling damage,");
 mvwprintz(w, 12, 39, c_white,       "loss of existing bionics, genetic damage");
 mvwprintz(w, 13, 39, c_white,       "or faulty installation.");
 wrefresh(w);

 if (type->id == "bio_power_storage" || type->id == "bio_power_storage_mkII") { // No selection list; just confirm
   int pow_up = BATTERY_AMOUNT;

   if (type->id == "bio_power_storage_mkII") {
     pow_up = 10;
   }

  mvwprintz(w, 3, 1, h_ltblue, "Power Storage +%d", pow_up);
  mvwprintz(w_description, 0, 0, c_ltblue, "\
Installing this bionic will increase your total power storage by %d.\n\
Power is necessary for most bionics to function. You also require a\n\
charge mechanism, which must be installed from another CBM.", pow_up);

  InputEvent input;
  wrefresh(w_description);
  wrefresh(w);
  do
   input = get_input();
  while (input != Confirm && input != Cancel);
  if (input == Confirm) {
   practice(g->turn, "electronics", (100 - chance_of_success) * 1.5);
   practice(g->turn, "firstaid", (100 - chance_of_success) * 1.0);
   practice(g->turn, "mechanics", (100 - chance_of_success) * 0.5);
   int success = chance_of_success - rng(1, 100);
   if (success > 0) {
    g->add_msg("Successfully installed batteries.");
    max_power_level += pow_up;
   } else
    bionics_install_failure(g, this, success);
   werase(w);
   delwin(w);
   g->refresh_all();
   return true;
  }
  werase(w);
  delwin(w);
  g->refresh_all();
  return false;
 }

 int selection = 0;
 InputEvent input;

 do {

  bionic_id bio_id = type->options[selection];
  mvwprintz(w, 3 + selection, 1, (has_bionic(bio_id) ? h_ltred : h_ltblue),
            bionics[bio_id]->name.c_str());

// Clear the bottom three lines...
  werase(w_description);
// ...and then fill them with the description of the selected bionic
  mvwprintz(w_description, 0, 0, c_ltblue, bionics[bio_id]->description.c_str());

  wrefresh(w_description);
  wrefresh(w);
  input = get_input();
  switch (input) {

  case DirectionS:
   mvwprintz(w, 2 + selection, 0, (has_bionic(bio_id) ? c_ltred : c_ltblue),
             bionics[bio_id]->name.c_str());
   if (selection == type->options.size() - 1)
    selection = 0;
   else
    selection++;
   break;

  case DirectionN:
   mvwprintz(w, 2 + selection, 0, (has_bionic(bio_id) ? c_ltred : c_ltblue),
             bionics[bio_id]->name.c_str());
   if (selection == 0)
    selection = type->options.size() - 1;
   else
    selection--;
   break;

  }
  if (input == Confirm && has_bionic(bio_id)) {
   popup("You already have a %s!", bionics[bio_id]->name.c_str());
   input = Nothing;
  }
 } while (input != Cancel && input != Confirm);

 if (input == Confirm) {
   practice(g->turn, "electronics", (100 - chance_of_success) * 1.5);
   practice(g->turn, "firstaid", (100 - chance_of_success) * 1.0);
   practice(g->turn, "mechanics", (100 - chance_of_success) * 0.5);
  bionic_id bio_id = type->options[selection];
  int success = chance_of_success - rng(1, 100);
  if (success > 0) {
   g->add_msg("Successfully installed %s.", bionics[bio_id]->name.c_str());
   add_bionic(bio_id);
  } else
   bionics_install_failure(g, this, success);
  werase(w);
  delwin(w);
  g->refresh_all();
  return true;
 }
 werase(w);
 delwin(w);
 g->refresh_all();
 return false;
}

void bionics_install_failure(game *g, player *u, int success)
{
 success = abs(success) - rng(1, 10);
 int failure_level = 0;
 if (success <= 0) {
  g->add_msg("The installation fails without incident.");
  return;
 }

 while (success > 0) {
  failure_level++;
  success -= rng(1, 10);
 }

 int fail_type = rng(1, (failure_level > 5 ? 5 : failure_level));
 std::string fail_text;

 switch (rng(1, 5)) {
  case 1: fail_text = "You flub the installation";	break;
  case 2: fail_text = "You mess up the installation";	break;
  case 3: fail_text = "The installation fails";		break;
  case 4: fail_text = "The installation is a failure";	break;
  case 5: fail_text = "You screw up the installation";	break;
 }

 if (fail_type == 3 && u->my_bionics.size() == 0)
  fail_type = 2; // If we have no bionics, take damage instead of losing some

 switch (fail_type) {

 case 1:
  fail_text += ", causing great pain.";
  u->pain += rng(failure_level * 3, failure_level * 6);
  break;

 case 2:
  fail_text += " and your body is damaged.";
  u->hurtall(rng(failure_level, failure_level * 2));
  break;

 case 3:
  fail_text += " and ";
  fail_text += (u->my_bionics.size() <= failure_level ? "all" : "some");
  fail_text += " of your existing bionics are lost.";
  for (int i = 0; i < failure_level && u->my_bionics.size() > 0; i++) {
   int rem = rng(0, u->my_bionics.size() - 1);
   u->my_bionics.erase(u->my_bionics.begin() + rem);
  }
  break;

 case 4:
  fail_text += " and do damage to your genetics, causing mutation.";
  g->add_msg(fail_text.c_str()); // Failure text comes BEFORE mutation text
  while (failure_level > 0) {
   u->mutate(g);
   failure_level -= rng(1, failure_level + 2);
  }
  return;	// So the failure text doesn't show up twice
  break;

 case 5:
 {
  fail_text += ", causing a faulty installation.";
  std::vector<bionic_id> valid;
  for (std::vector<std::string>::iterator it = faulty_bionics.begin() ; it != faulty_bionics.end(); ++it){
   if (!u->has_bionic(*it)){
    valid.push_back(*it);
   }
  }
  if (valid.size() == 0) {	// We've got all the bad bionics!
   if (u->max_power_level > 0) {
    g->add_msg("You lose power capacity!");
    u->max_power_level = rng(0, u->max_power_level - 1);
   }
// TODO: What if we can't lose power capacity?  No penalty?
  } else {
   int index = rng(0, valid.size() - 1);
   u->add_bionic(valid[index]);
  }
 }
  break;
 }

 g->add_msg(fail_text.c_str());

}

void game::init_bionics()
{
    catajson bionics_file("data/raw/bionics.json");

    for(bionics_file.set_begin(); bionics_file.has_curr(); bionics_file.next())
    {
        catajson bio = bionics_file.curr();

        std::set<std::string> tags;

        if(bio.has("flags"))
        {
            tags = bio.get("flags").as_tags();
        }

        // set up all the bionic parameters
        std::string id          = bio.get("id").as_string();
        std::string name        = bio.get("name").as_string();
        int cost                = bio.get("cost").as_int();
        int time                = bio.get("time").as_int();
        std::string description = bio.get("description").as_string();
        bool faulty             = (tags.find("FAULTY") != tags.end());
        bool powersource        = (tags.find("POWER") != tags.end());
        bool active             = (tags.find("ACTIVE") != tags.end());

        bionics[id] = new bionic_data(name, powersource, active, cost, time, description, faulty);

        // Don't add bio_null to any vectors.
        if(id != "bio_null")
        {
            if(faulty)
            {
                faulty_bionics.push_back(id);
            }
            else if(powersource)
            {
                power_source_bionics.push_back(id);
            }
            else if(!active)
            {
                unpowered_bionics.push_back(id);
            }
        }

    }
}

