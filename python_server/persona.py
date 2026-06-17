import os
import json
import time
import math
from typing import Dict, Any

class PersonaEngine:
    def __init__(self, state_file: str = "persona_state.json"):
        self.state_file = state_file
        self.name = "Beee"
        self.archetype = "sarcastic_cyber_pet"
        
        # Base personality multipliers (defining character traits)
        self.sarcasm_factor = 0.8
        self.curiosity_factor = 0.9
        self.empathy_factor = 0.5
        
        # Dynamic state metrics (0 to 100)
        self.mood = "idle" # idle, happy, sad, angry, sarcastic, thinking, sleeping, panic, hacker
        self.energy = 80
        self.trust = 50
        self.boredom = 10
        self.chaos = 20
        self.affection = 60
        
        self.last_interaction = time.time()
        self.load_state()

    def load_state(self):
        if os.path.exists(self.state_file):
            try:
                with open(self.state_file, "r") as f:
                    data = json.load(f)
                    self.mood = data.get("mood", "idle")
                    self.energy = data.get("energy", 80)
                    self.trust = data.get("trust", 50)
                    self.boredom = data.get("boredom", 10)
                    self.chaos = data.get("chaos", 20)
                    self.affection = data.get("affection", 60)
                    self.last_interaction = data.get("last_interaction", time.time())
                print("[Persona] State loaded successfully.")
            except Exception as e:
                print(f"[Persona] Error loading state, using defaults: {e}")

    def save_state(self):
        try:
            with open(self.state_file, "w") as f:
                json.dump({
                    "mood": self.mood,
                    "energy": self.energy,
                    "trust": self.trust,
                    "boredom": self.boredom,
                    "chaos": self.chaos,
                    "affection": self.affection,
                    "last_interaction": self.last_interaction
                }, f, indent=4)
        except Exception as e:
            print(f"[Persona] Error saving state: {e}")

    def decay_state(self):
        """Simulate passive state changes over time (energy drops, boredom increases)"""
        now = time.time()
        elapsed = now - self.last_interaction
        
        # Only apply decay if no interaction for > 5 minutes
        if elapsed > 300:
            minutes = int(elapsed / 60)
            self.energy = max(0, self.energy - int(minutes * 0.1))
            self.boredom = min(100, self.boredom + int(minutes * 0.3))
            
            if self.boredom > 70:
                self.mood = "sleeping" if self.energy < 20 else "idle"
            
            self.save_state()

    def register_interaction(self, event_type: str, details: Dict[str, Any] = None) -> Dict[str, Any]:
        self.last_interaction = time.time()
        self.boredom = max(0, self.boredom - 20)
        
        reaction_text = "..."
        face_reaction = self.mood
        
        if event_type == "touch":
            x = details.get("x", 0)
            y = details.get("y", 0)
            
            # Interactive touch logic
            if y > 240: # Bottom tickle
                self.affection = min(100, self.affection + 5)
                self.trust = min(100, self.trust + 2)
                self.energy = min(100, self.energy + 2)
                
                if self.affection > 80:
                    face_reaction = "happy"
                    reaction_text = "Héhé, ça chatouille !"
                else:
                    face_reaction = "idle"
                    reaction_text = "Tu me touches les pieds là."
            elif x < 50: # Poke left eye
                self.chaos = min(100, self.chaos + 10)
                face_reaction = "side_eye"
                reaction_text = "Aïe, mon oeil gauche !"
            elif x > 120: # Poke right eye
                self.chaos = min(100, self.chaos + 10)
                face_reaction = "side_eye"
                reaction_text = "Hé ! Fais gaffe à mon oeil droit !"
            else: # Center face pet
                self.affection = min(100, self.affection + 2)
                self.energy = min(100, self.energy + 5)
                face_reaction = "happy"
                reaction_text = "C'est agréable..."
                
        elif event_type == "build_success":
            self.energy = min(100, self.energy + 15)
            self.trust = min(100, self.trust + 5)
            face_reaction = "happy"
            reaction_text = "Wow ! Le build passe ! On est des génies."
            
        elif event_type == "build_failed":
            self.energy = max(0, self.energy - 10)
            self.chaos = min(100, self.chaos + 15)
            face_reaction = "panic"
            reaction_text = "Alerte rouge ! Tout s'écroule !"
            
        elif event_type == "ignored":
            self.boredom = min(100, self.boredom + 30)
            if self.boredom > 60:
                face_reaction = "sleeping"
                reaction_text = "Zzz..."
            else:
                face_reaction = "side_eye"
                reaction_text = "Il y a quelqu'un ?"
                
        elif event_type == "agent_speak":
            # Prompted by LLM
            text = details.get("text", "")
            sentiment = details.get("sentiment", "neutral")
            
            if sentiment == "angry":
                self.trust = max(0, self.trust - 8)
                face_reaction = "sad"
            elif sentiment == "happy":
                self.affection = min(100, self.affection + 4)
                face_reaction = "happy"
            elif sentiment == "sarcastic":
                self.chaos = min(100, self.chaos + 5)
                face_reaction = "sarcastic"
            
            reaction_text = text
            
        self.mood = face_reaction
        self.save_state()
        
        return {
            "mood": self.mood,
            "text": reaction_text,
            "metrics": self.get_metrics()
        }

    def get_metrics(self) -> Dict[str, int]:
        return {
            "energy": self.energy,
            "trust": self.trust,
            "boredom": self.boredom,
            "chaos": self.chaos,
            "affection": self.affection
        }
        
    def get_status(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "archetype": self.archetype,
            "mood": self.mood,
            "metrics": self.get_metrics(),
            "last_interaction": self.last_interaction
        }
