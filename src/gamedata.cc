#include "gamedata.h"

GameData::GameData() {

}

GameData::~GameData() {

}

Module *GameData::get_module(int id) {
  for(vector<Module>::iterator it = modules.begin(); it != modules.end(); ++it) {
    if(it->getArbId() == id) return &*it;
  }  
  return NULL;
}

vector <Module *> GameData::get_active_modules() {
  vector<Module *> active_modules;
  for(vector<Module>::iterator it = modules.begin(); it != modules.end(); ++it) {
    if(it->isResponder() == false) active_modules.push_back(&*it);
  }  
  return active_modules;
}


Module *GameData::get_possible_module(int id) {
  for(vector<Module>::iterator it = possible_modules.begin(); it != possible_modules.end(); ++it) {
    if(it->getArbId() == id) return &*it;
  }  
  return NULL;
}

void GameData::setMode(int m) {
  switch(m) {
    case MODE_SIM:
      Msg("Switching to Simulator mode");
      if(mode == MODE_LEARN) { // Previous mode was learning, update
        Msg("Normalizing learned data");
        GameData::processLearned();
      }
      mode=m;
      break;
    case MODE_LEARN:
      Msg("Switching to Learning mode");
      mode=m;
      break;
    default:
      Msg("Unknown game mode");
      break;
  }
}

void GameData::processPkt(canfd_frame *cf) {
  switch(mode) {
    case MODE_SIM:
      GameData::HandleSim(cf);
      break;
    case MODE_LEARN:
      GameData::LearnPacket(cf);
      break;
    default:
      cout << "ERROR: Processing packets while in an unknown mode" << mode << endl;
      break;
  }
}

void GameData::HandleSim(canfd_frame *cf) {

}

void GameData::LearnPacket(canfd_frame *cf) {
  Module *module = GameData::get_possible_module(cf->can_id);
  Module *possible_module = GameData::isPossibleISOTP(cf);
  int possible;
  if(module) {
    module->addPacket(cf);
    if(possible_module) {
      module->incMatchedISOTP();
    } else {
      // Still maybe an ISOTP answer, check for common syntax
      if(cf->data[0] == 0x10 && cf->len == 8) {
        module->incMatchedISOTP();
      } else if(cf->data[0] == 0x30 && cf->len == 3) {
        module->incMatchedISOTP();
      } else if(cf->data[0] >= 0x21 || cf->data[0] <= 0x30) {
        module->incMatchedISOTP();
      } else {
        module->incMissedISOTP();
      }
    }
  } else if(possible_module) { // Haven't seen this ID yet
    possible_module->addPacket(cf);
    possible_modules.push_back(*possible_module);
  }
}

Module *GameData::isPossibleISOTP(canfd_frame *cf) {
  int i;
  bool padding = false;
  char last_byte;
  Module *possible = NULL;
    if(cf->data[0] == cf->len - 1) { // Possible UDS request
      possible = new Module(cf->can_id);
    } else if(cf->data[0] < cf->len - 2) { // Check if remaining bytes are just padding
      padding = true;
      if(cf->data[0] == 0) padding = false;
      last_byte = cf->data[cf->data[0] + 1];
      for(i=cf->data[0] + 2; i < cf->len; i++) {
        if(cf->data[i] != last_byte) {
          padding = false;
        } else {
          last_byte = cf->data[i];
        }
      }
      if(padding == true) { // Possible UDS w/ padding
        possible = new Module(cf->can_id);
        possible->setPaddingByte(last_byte);
      }
    }
  return possible;
}

void GameData::processLearned() {
  if(verbose) cout << "Identified " << possible_modules.size() << " possible modules" << endl;
  for(vector<Module>::iterator it = possible_modules.begin(); it != possible_modules.end(); ++it) {
    if(it->confidence() > CONFIDENCE_THRESHOLD) {
      if(verbose) cout << "ID: " << hex << it->getArbId() << " Looks like a UDS compliant module" << endl;
      modules.push_back(*it);
    }
  } 
  if(verbose) cout << "Locating responders" << endl;
  Module *responder = NULL;
  for(vector<Module>::iterator it = modules.begin(); it != modules.end(); ++it) {
     if(it->isResponder() == false) {
       responder = GameData::get_module(it->getArbId() + 0x300);
       if(responder) { // GM style positive response
         it->setPositiveResponse(responder);
         responder->setResponder(true);
       }
       responder = GameData::get_module(it->getArbId() + 0x400);
       if(responder) { // GM style negative response
         it->setNegativeResponse(responder);
         responder->setResponder(true);
       }
       responder = GameData::get_module(it->getArbId() + 0x08);
       if(responder) { // Standard response
         it->setPositiveResponse(responder);
         it->setNegativeResponse(responder);
         responder->setResponder(true);
       }
     }
  }
  stringstream m;
  m << "Identified " << GameData::get_active_modules().size() << " Active modules";
  GameData::Msg(m.str());
}

string GameData::frame2string(canfd_frame *cf) {
  stringstream pkt;
  pkt << hex << cf->can_id << CANID_DELIM;
  int i;
  for(i=0; i < cf->len; i++) {
    pkt << setfill('0') << setw(2) << hex << (int)cf->data[i];
  }
  return pkt.str();
}

void GameData::Msg(string mesg) {
  if(_gui == NULL) return;
  _gui->Msg(mesg);
}

bool GameData::SaveConfig() {
  ofstream configFile;
  configFile.open("config_data.cfg");
  // Globals
  // Modules
  configFile << endl;
  vector<Module *>modules = GameData::get_active_modules();
  for(vector<Module *>::iterator it = modules.begin(); it != modules.end(); ++it) {
    Module *mod = *it;
    configFile << "[" << hex << mod->getArbId() << "]" << endl;
    configFile << "pos = " << dec << mod->getX() << "," << mod->getY() << endl;
  }
  configFile.close();
  Msg("Saved config_data.cfg");
  return true;
}

int string2hex(string s) {
  stringstream ss;
  int h;
  ss << hex << s;
  ss >> h;
  return h;
}

int string2int(string s) {
  stringstream ss;
  int i;
  ss << dec << s;
  ss >> i;
  return i;
}
