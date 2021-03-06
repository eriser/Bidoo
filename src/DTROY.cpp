#include "Bidoo.hpp"
#include "dsp/digital.hpp"
#include "BidooComponents.hpp"
#include <ctime>
#include <iostream>
#include <vector>
#include <random>

using namespace std;

struct DTROY : Module {
	enum ParamIds {
		CLOCK_PARAM,
		RUN_PARAM,
		RESET_PARAM,
		STEPS_PARAM,
		SLIDE_TIME_PARAM,
		GATE_TIME_PARAM,
		ROOT_NOTE_PARAM,
		SCALE_PARAM,
		PLAY_MODE_PARAM,
		TRIG_COUNT_PARAM = GATE_TIME_PARAM + 8,
		TRIG_TYPE_PARAM = TRIG_COUNT_PARAM + 8,
		TRIG_PITCH_PARAM = TRIG_TYPE_PARAM + 8,
		TRIG_SLIDE_PARAM = TRIG_PITCH_PARAM + 8,
		TRIG_SKIP_PARAM = TRIG_SLIDE_PARAM + 8,
		NUM_PARAMS = TRIG_SKIP_PARAM + 8
	};
	enum InputIds {
		CLOCK_INPUT,
		EXT_CLOCK_INPUT,
		RESET_INPUT,
		STEPS_INPUT,
		SLIDE_TIME_INPUT,
		GATE_TIME_INPUT,
		ROOT_NOTE_INPUT,
		SCALE_INPUT,
		EXTGATE1_INPUT,
		EXTGATE2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATE_OUTPUT,
		PITCH_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		RUNNING_LIGHT,
		RESET_LIGHT,
		GATE_LIGHT,
		STEPS_LIGHTS = GATE_LIGHT + 8,
		SLIDES_LIGHTS = STEPS_LIGHTS + 8,
		SKIPS_LIGHTS = SLIDES_LIGHTS + 8,
		NUM_LIGHTS = SKIPS_LIGHTS + 8
	};
	
	//copied from http://www.grantmuller.com/MidiReference/doc/midiReference/ScaleReference.html
	int SCALE_AEOLIAN        [7] = {0, 2, 3, 5, 7, 8, 10};
	int SCALE_BLUES          [9] = {0, 2, 3, 4, 5, 7, 9, 10, 11};
	int SCALE_CHROMATIC      [12]= {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
	int SCALE_DIATONIC_MINOR [7] = {0, 2, 3, 5, 7, 8, 10};
	int SCALE_DORIAN         [7] = {0, 2, 3, 5, 7, 9, 10};
	int SCALE_HARMONIC_MINOR [7] = {0, 2, 3, 5, 7, 8, 11};
	int SCALE_INDIAN         [7] = {0, 1, 1, 4, 5, 8, 10};
	int SCALE_LOCRIAN        [7] = {0, 1, 3, 5, 6, 8, 10};
	int SCALE_LYDIAN         [7] = {0, 2, 4, 6, 7, 9, 10};
	int SCALE_MAJOR          [7] = {0, 2, 4, 5, 7, 9, 11};
	int SCALE_MELODIC_MINOR  [9] = {0, 2, 3, 5, 7, 8, 9, 10, 11};
	int SCALE_MINOR          [7] = {0, 2, 3, 5, 7, 8, 10};
	int SCALE_MIXOLYDIAN     [7] = {0, 2, 4, 5, 7, 9, 10};
	int SCALE_NATURAL_MINOR  [7] = {0, 2, 3, 5, 7, 8, 10};
	int SCALE_PENTATONIC     [5] = {0, 2, 4, 7, 9};
	int SCALE_PHRYGIAN       [7] = {0, 1, 3, 5, 7, 8, 10};
	int SCALE_TURKISH        [7] = {0, 1, 3, 5, 7, 10, 11};
	
	enum Notes {
		NOTE_C, 
		NOTE_C_SHARP,
		NOTE_D,
		NOTE_D_SHARP,
		NOTE_E,
		NOTE_F,
		NOTE_F_SHARP,
		NOTE_G,
		NOTE_G_SHARP,
		NOTE_A,
		NOTE_A_SHARP,
		NOTE_B,
		NUM_NOTES
	};
	
	enum Scales {
		AEOLIAN,
		BLUES,
		CHROMATIC,
		DIATONIC_MINOR,
		DORIAN,
		HARMONIC_MINOR,
		INDIAN,
		LOCRIAN,
		LYDIAN,
		MAJOR,
		MELODIC_MINOR,
		MINOR,
		MIXOLYDIAN,
		NATURAL_MINOR,
		PENTATONIC,
		PHRYGIAN,
		TURKISH,
		NONE,
		NUM_SCALES
	};

	bool running = true;
	SchmittTrigger clockTrigger; // for external clock
	// For buttons
	SchmittTrigger runningTrigger;
	SchmittTrigger resetTrigger;
	SchmittTrigger slideTriggers[8];
	SchmittTrigger skipTriggers[8];
	SchmittTrigger playModeTrigger;
	float phase = 0;
	int index = 0;
	int subIndex = 0;
	bool reStart = true;
	int floor = 0;
	int rootNote = 0;
	int curScaleVal = 0;
	float pitch = 0;
	float previousPitch = 0;
	clock_t tCurrent;
	clock_t tLastTrig;
	clock_t tPreviousTrig;
	bool slideState[8] = {0};
	bool skipState[8] = {0};
	int playMode = 0; // 0 forward, 1 backward, 2 pingpong, 3 random, 4 brownian
	int pingPongMem = 0;
	int countSteps = 0;
	
	vector<int> pattern; 
	vector<int> subPattern;
	

	DTROY() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {	}
	
	void step() override;
	
	// persistence, random & init

	json_t *toJson() override {
		json_t *rootJ = json_object();

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		//playMode
		json_object_set_new(rootJ, "playMode", json_integer(playMode));

		// slides
		json_t *slidesJ = json_array();
		for (int i = 0; i < 8; i++) {
			json_t *slideJ = json_integer((int) slideState[i]);
			json_array_append_new(slidesJ, slideJ);
		}
		json_object_set_new(rootJ, "slides", slidesJ);
		
		// skips
		json_t *skipsJ = json_array();
		for (int i = 0; i < 8; i++) {
			json_t *skipJ = json_integer((int) skipState[i]);
			json_array_append_new(skipsJ, skipJ);
		}
		json_object_set_new(rootJ, "skips", skipsJ);

		return rootJ;
	}

	void fromJson(json_t *rootJ) override {
		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);
		
		// playMode
		json_t *playModeJ = json_object_get(rootJ, "playMode");
		if (playModeJ)
			playMode = json_integer_value(playModeJ);

		// Slides
		json_t *slidesJ = json_object_get(rootJ, "slides");
		if (slidesJ) {
			for (int i = 0; i < 8; i++) {
				json_t *slideJ = json_array_get(slidesJ, i);
				if (slideJ)
					slideState[i] = json_integer_value(slideJ);
			}
		}
		
		// Skips
		json_t *skipsJ = json_object_get(rootJ, "skips");
		if (skipsJ) {
			for (int i = 0; i < 8; i++) {
				json_t *skipJ = json_array_get(skipsJ, i);
				if (skipJ)
					skipState[i] = json_integer_value(skipJ);
			}
		}
	}
	
	void randomize() override {
		for (int i = 0; i < 8; i++) {
			slideState[i] = (randomf() > 0.8);
			skipState[i] = (randomf() > 0.85);
		}
	}

		
	// pattern utilities	
		
	int numSteps() { return clampi(roundf(params[STEPS_PARAM].value + inputs[STEPS_INPUT].value), 1, 16); }
	
	string getPattern () {
		string result = "";
		for (unsigned int i = 0; i<pattern.size(); i++){
			result = result + " " + std::to_string(pattern.at(i));
		}
		return result;
	}
	
	template<typename Iter, typename RandomGenerator> Iter select_randomly(Iter start, Iter end, RandomGenerator& g) {
		std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
		std::advance(start, dis(g));
		return start;
	}

	template<typename Iter> Iter select_randomly(Iter start, Iter end) {
		static std::random_device rd;
		static std::mt19937 gen(rd());
		return select_randomly(start, end, gen);
	}

	
	// Quantization inspired from  https://github.com/jeremywen/JW-Modules
	
	float getOneRandomNoteInScale(){
		rootNote = clampi(params[ROOT_NOTE_PARAM].value + inputs[ROOT_NOTE_INPUT].value, 0.0, NUM_NOTES-1);
		curScaleVal = clampi(params[SCALE_PARAM].value + inputs[SCALE_INPUT].value, 0.0, NUM_SCALES-1);
		int *curScaleArr;
		int notesInScale = 0;
		switch(curScaleVal){
			case AEOLIAN:        curScaleArr = SCALE_AEOLIAN;       notesInScale=LENGTHOF(SCALE_AEOLIAN); break;
			case BLUES:          curScaleArr = SCALE_BLUES;         notesInScale=LENGTHOF(SCALE_BLUES); break;
			case CHROMATIC:      curScaleArr = SCALE_CHROMATIC;     notesInScale=LENGTHOF(SCALE_CHROMATIC); break;
			case DIATONIC_MINOR: curScaleArr = SCALE_DIATONIC_MINOR;notesInScale=LENGTHOF(SCALE_DIATONIC_MINOR); break;
			case DORIAN:         curScaleArr = SCALE_DORIAN;        notesInScale=LENGTHOF(SCALE_DORIAN); break;
			case HARMONIC_MINOR: curScaleArr = SCALE_HARMONIC_MINOR;notesInScale=LENGTHOF(SCALE_HARMONIC_MINOR); break;
			case INDIAN:         curScaleArr = SCALE_INDIAN;        notesInScale=LENGTHOF(SCALE_INDIAN); break;
			case LOCRIAN:        curScaleArr = SCALE_LOCRIAN;       notesInScale=LENGTHOF(SCALE_LOCRIAN); break;
			case LYDIAN:         curScaleArr = SCALE_LYDIAN;        notesInScale=LENGTHOF(SCALE_LYDIAN); break;
			case MAJOR:          curScaleArr = SCALE_MAJOR;         notesInScale=LENGTHOF(SCALE_MAJOR); break;
			case MELODIC_MINOR:  curScaleArr = SCALE_MELODIC_MINOR; notesInScale=LENGTHOF(SCALE_MELODIC_MINOR); break;
			case MINOR:          curScaleArr = SCALE_MINOR;         notesInScale=LENGTHOF(SCALE_MINOR); break;
			case MIXOLYDIAN:     curScaleArr = SCALE_MIXOLYDIAN;    notesInScale=LENGTHOF(SCALE_MIXOLYDIAN); break;
			case NATURAL_MINOR:  curScaleArr = SCALE_NATURAL_MINOR; notesInScale=LENGTHOF(SCALE_NATURAL_MINOR); break;
			case PENTATONIC:     curScaleArr = SCALE_PENTATONIC;    notesInScale=LENGTHOF(SCALE_PENTATONIC); break;
			case PHRYGIAN:       curScaleArr = SCALE_PHRYGIAN;      notesInScale=LENGTHOF(SCALE_PHRYGIAN); break;
			case TURKISH:        curScaleArr = SCALE_TURKISH;       notesInScale=LENGTHOF(SCALE_TURKISH); break;
		}

		if(curScaleVal == NONE){
			return randomf() * 6.0;
		} else {
			float voltsOut = 0;
			int rndOctaveInVolts = int(5 * randomf());
			voltsOut += rndOctaveInVolts;
			voltsOut += rootNote / 12.0;
			voltsOut += curScaleArr[int(notesInScale * randomf())] / 12.0;
			return voltsOut;
		}
	}

	float closestVoltageInScale(float voltsIn){
		rootNote = clampi(params[ROOT_NOTE_PARAM].value + inputs[ROOT_NOTE_INPUT].value, 0.0, DTROY::NUM_NOTES-1);
		curScaleVal = clampi(params[SCALE_PARAM].value + inputs[SCALE_INPUT].value, 0.0, DTROY::NUM_SCALES-1);
		int *curScaleArr;
		int notesInScale = 0;
		switch(curScaleVal){
			case AEOLIAN:        curScaleArr = SCALE_AEOLIAN;       notesInScale=LENGTHOF(SCALE_AEOLIAN); break;
			case BLUES:          curScaleArr = SCALE_BLUES;         notesInScale=LENGTHOF(SCALE_BLUES); break;
			case CHROMATIC:      curScaleArr = SCALE_CHROMATIC;     notesInScale=LENGTHOF(SCALE_CHROMATIC); break;
			case DIATONIC_MINOR: curScaleArr = SCALE_DIATONIC_MINOR;notesInScale=LENGTHOF(SCALE_DIATONIC_MINOR); break;
			case DORIAN:         curScaleArr = SCALE_DORIAN;        notesInScale=LENGTHOF(SCALE_DORIAN); break;
			case HARMONIC_MINOR: curScaleArr = SCALE_HARMONIC_MINOR;notesInScale=LENGTHOF(SCALE_HARMONIC_MINOR); break;
			case INDIAN:         curScaleArr = SCALE_INDIAN;        notesInScale=LENGTHOF(SCALE_INDIAN); break;
			case LOCRIAN:        curScaleArr = SCALE_LOCRIAN;       notesInScale=LENGTHOF(SCALE_LOCRIAN); break;
			case LYDIAN:         curScaleArr = SCALE_LYDIAN;        notesInScale=LENGTHOF(SCALE_LYDIAN); break;
			case MAJOR:          curScaleArr = SCALE_MAJOR;         notesInScale=LENGTHOF(SCALE_MAJOR); break;
			case MELODIC_MINOR:  curScaleArr = SCALE_MELODIC_MINOR; notesInScale=LENGTHOF(SCALE_MELODIC_MINOR); break;
			case MINOR:          curScaleArr = SCALE_MINOR;         notesInScale=LENGTHOF(SCALE_MINOR); break;
			case MIXOLYDIAN:     curScaleArr = SCALE_MIXOLYDIAN;    notesInScale=LENGTHOF(SCALE_MIXOLYDIAN); break;
			case NATURAL_MINOR:  curScaleArr = SCALE_NATURAL_MINOR; notesInScale=LENGTHOF(SCALE_NATURAL_MINOR); break;
			case PENTATONIC:     curScaleArr = SCALE_PENTATONIC;    notesInScale=LENGTHOF(SCALE_PENTATONIC); break;
			case PHRYGIAN:       curScaleArr = SCALE_PHRYGIAN;      notesInScale=LENGTHOF(SCALE_PHRYGIAN); break;
			case TURKISH:        curScaleArr = SCALE_TURKISH;       notesInScale=LENGTHOF(SCALE_TURKISH); break;
			case NONE:           return voltsIn;
		}

		float closestVal = 10.0;
		float closestDist = 10.0;
		int octaveInVolts = int(voltsIn);
		for (int i = 0; i < notesInScale; i++) {
			float scaleNoteInVolts = octaveInVolts + ((rootNote + curScaleArr[i]) / 12.0);
			float distAway = fabs(voltsIn - scaleNoteInVolts);
			if(distAway < closestDist){
				closestVal = scaleNoteInVolts;
				closestDist = distAway;
			}
		}
		return closestVal;
	}
};


void DTROY::step() {
 	const float lightLambda = 0.075;
	// Run
	if (runningTrigger.process(params[RUN_PARAM].value)) {
		running = !running;
	}
	lights[RUNNING_LIGHT].value = running ? 1.0 : 0.0;

	bool nextStep = false;

	// Phase calculation
	if (running) {
		if (inputs[EXT_CLOCK_INPUT].active) {
			tCurrent = clock();
			float clockTime = powf(2.0, params[CLOCK_PARAM].value + inputs[CLOCK_INPUT].value);
			if (tLastTrig && tPreviousTrig) {
				phase = float(tCurrent - tLastTrig) / float(tLastTrig - tPreviousTrig);
			} 
			else {
				phase += clockTime / engineGetSampleRate();
			}
			// External clock
			if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].value)) {
				tPreviousTrig = tLastTrig;
				tLastTrig = clock();
				phase = 0.0;
				nextStep = true;
			}
		}
		else {
			// Internal clock
			float clockTime = powf(2.0, params[CLOCK_PARAM].value + inputs[CLOCK_INPUT].value);
			phase += clockTime / engineGetSampleRate();
			if (phase >= 1.0) {
				phase--;
				nextStep = true;
			}
		}
	}

	// Reset
	if (resetTrigger.process(params[RESET_PARAM].value + inputs[RESET_INPUT].value)) {
		phase = 0.0;
		floor = 0;
		countSteps = numSteps();
		lights[RESET_LIGHT].value = 1.0;
	}
	
	// Trigs Update
	for (int i = 0; i < 8; i++) {	
		if (slideTriggers[i].process(params[TRIG_SLIDE_PARAM + i].value)) {
			slideState[i] = !slideState[i];
		}
		if (skipTriggers[i].process(params[TRIG_SKIP_PARAM + i].value)) {
			skipState[i] = !skipState[i];
		} 
	}
	
	// PlayMode
	if (playModeTrigger.process(params[PLAY_MODE_PARAM].value)) {
		playMode = (((int)playMode + 1) % 5);
	}
	
	// Pattern
	vector<int>().swap(pattern);
	if (playMode == 1)
	{
		int count = 0;
		for (int i=7; i >= 0; i = i - 1) {
			if (!skipState[i%8]) {
				pattern.push_back(i);
			}
			count++;
			if (count == numSteps())
				break;
		}
		if (numSteps() > 8) {
			for (int i=7; i >= 0; i = i - 1) {
				if (!skipState[i%8]){
					pattern.push_back(i);
				}
				count++;
				if (count == numSteps())
					break;
			}
		}
	}
	else if (playMode == 2)
	{
		for (int i=0; i < numSteps(); i = i + 1){
			if (!skipState[i%8]){
				pattern.push_back(i);
			}
		}
		bool first = true;
		for (int i=numSteps()-1; i > 0; i = i - 1){
			if (!skipState[i%8]){
				if (first) 
					first = false;
				else
					pattern.push_back(i);
			}
		}
		if ((pattern.size()>1) && (pattern.back() == pattern.front()))
			pattern.pop_back();
	}
	else
	{
		for (int i=0; i < numSteps(); i = i + 1){
			if (!skipState[i%8]){
				pattern.push_back(i);
			}
		}
	}
		
		
	// Steps && Floors Management
	if (nextStep) {
		// Advance step
		
		if (floor < roundf(params[TRIG_COUNT_PARAM + index%8].value - 1)) {
			floor += 1;
		}
		else 
		{
			previousPitch = closestVoltageInScale(params[TRIG_PITCH_PARAM + index%8].value);
			floor = 0;
			if (playMode == 0) 
			{
				countSteps++;
				
				if ((countSteps > numSteps()) || ((unsigned int)countSteps > pattern.size() - 1))
					countSteps = 0;
				
				if (pattern.size() == 0)
					index = 0;
				else
					index = pattern.at(countSteps);

			}
			if (playMode == 1) 
			{
				countSteps++;
				
				if ((countSteps > numSteps()) || ((unsigned int)countSteps > pattern.size() - 1))
					countSteps = 0;
				
				if (pattern.size() == 0)
					index = 7;
				else
					index = pattern.at(countSteps);
			}
			if (playMode == 2) 
			{
				countSteps++;
				
				if ((countSteps > 2 * numSteps() - 2 ) || ((unsigned int)countSteps > pattern.size() - 1))
					countSteps = 0;
				
				if (pattern.size() == 0)
					index = 0;
				else
					index = pattern.at(countSteps);
			}
			else if (playMode == 3)
			{	
				if (pattern.size() == 0) {
					index = 0;
				}
				else {
					index = *select_randomly(pattern.begin(), pattern.end());
				}
			}
			else if (playMode == 4)
			{		
				if (pattern.size() == 0) {
					index = 0;
				}
				else {
					vector<int>().swap(subPattern);
					subPattern.push_back(pattern.at(clampi((subIndex-1)+(subIndex <= 0 ? pattern.size() : 0),0,pattern.size()-1)));
					subPattern.push_back(pattern.at(clampi((subIndex+1)%pattern.size(),0,pattern.size()-1)));
					index = *select_randomly(subPattern.begin(), subPattern.end());
					if (index == subPattern.front()) {
						subIndex = (subIndex-1)+(subIndex <= 0 ? pattern.size() : 0);
					}
					else {
						subIndex = (subIndex+1)%pattern.size();			
					}
				}
			}
		}
		lights[STEPS_LIGHTS+index%8].value = 1.0;
	}


	// Lights
	for (int i = 0; i < 8; i++) {
		lights[STEPS_LIGHTS + i].value -= lights[STEPS_LIGHTS + i].value / lightLambda / engineGetSampleRate();	
		lights[SLIDES_LIGHTS + i].value = slideState[i] ? 1 - lights[STEPS_LIGHTS + i].value : lights[STEPS_LIGHTS + i].value;
		lights[SKIPS_LIGHTS + i].value = skipState[i] ? 1 - lights[STEPS_LIGHTS + i].value : lights[STEPS_LIGHTS + i].value;
	}
	lights[RESET_LIGHT].value -= lights[RESET_LIGHT].value / lightLambda / engineGetSampleRate();

	// Caclulate Outputs
	bool gateOn = running && !skipState[index%8];
	float gateValue = 0.0;
	if (gateOn){
		if (roundf(params[TRIG_TYPE_PARAM + index%8].value) == 0) {
			gateOn = false;
		}
		else if (((roundf(params[TRIG_TYPE_PARAM + index%8].value) == 1) && (floor == 0)) 
				|| (roundf(params[TRIG_TYPE_PARAM + index%8].value) == 2)
				|| ((roundf(params[TRIG_TYPE_PARAM + index%8].value) == 3) && (floor == roundf(params[TRIG_COUNT_PARAM + index%8].value)))) {
				float gateCoeff = clampf(params[GATE_TIME_PARAM].value - 0.02 + inputs[GATE_TIME_INPUT].value /10, 0.0, 0.99);
			gateOn = phase < gateCoeff;
			gateValue = 10.0;
		}
		else if (roundf(params[TRIG_TYPE_PARAM + index%8].value) == 3) {
			gateOn = true;
			gateValue = 10.0;
		}
		else if (roundf(params[TRIG_TYPE_PARAM + index%8].value) == 4) {
			gateOn = true;
			gateValue = inputs[EXTGATE1_INPUT].value;
		}
		else if (roundf(params[TRIG_TYPE_PARAM + index%8].value) == 5) {
			gateOn = true;
			gateValue = inputs[EXTGATE2_INPUT].value;
		}
		else {
			gateOn = false;
			gateValue = 0.0;
		}
	}
	
	pitch = closestVoltageInScale(params[TRIG_PITCH_PARAM + index%8].value);
	if (slideState[index%8]) {
		if (floor == 0) {
			float slideCoeff = clampf(params[SLIDE_TIME_PARAM].value - 0.01 + inputs[SLIDE_TIME_INPUT].value /10, -0.1, 0.99);
			pitch = pitch - (1 - powf(phase, slideCoeff)) * (pitch - previousPitch);
		}
	}
			
	// Update Outputs
	outputs[GATE_OUTPUT].value = gateOn ? gateValue : 0.0;
	outputs[PITCH_OUTPUT].value = pitch;
}

struct DTROYDisplay : TransparentWidget {
	DTROY *module;
	int frame = 0;
	shared_ptr<Font> font;

	string note, scale, steps, playMode, pattern;

	DTROYDisplay() {
		font = Font::load(assetPlugin(plugin, "res/DejaVuSansMono.ttf"));
	}

	void drawMessage(NVGcontext *vg, Vec pos, string note, string playMode, string steps, string scale, string pattern) {
		nvgFontSize(vg, 18);
		nvgFontFaceId(vg, font->handle);
		nvgTextLetterSpacing(vg, -2);
		nvgFillColor(vg, nvgRGBA(75, 199, 75, 0xff));
		nvgText(vg, pos.x + 7, pos.y + 8, playMode.c_str(), NULL);
		nvgFontSize(vg, 14);
		nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0xff));
		nvgText(vg, pos.x + 75, pos.y + 7, steps.c_str(), NULL);	
		nvgText(vg, pos.x + 8, pos.y + 23, note.c_str(), NULL);		
		nvgText(vg, pos.x + 30, pos.y + 23, scale.c_str(), NULL);
		/* nvgFillColor(vg, nvgRGBA(0x00, 0x00, 0x00, 0xff));
		nvgTextLetterSpacing(vg, 0);
		nvgText(vg,180, 105, pattern.c_str(), NULL); */
	}
	
	string displayRootNote(int value) { 
		switch(value){
			case DTROY::NOTE_C:       return "C";
			case DTROY::NOTE_C_SHARP: return "C#";
			case DTROY::NOTE_D:       return "D";
			case DTROY::NOTE_D_SHARP: return "D#";
			case DTROY::NOTE_E:       return "E";
			case DTROY::NOTE_F:       return "F";
			case DTROY::NOTE_F_SHARP: return "F#";
			case DTROY::NOTE_G:       return "G";
			case DTROY::NOTE_G_SHARP: return "G#";
			case DTROY::NOTE_A:       return "A";
			case DTROY::NOTE_A_SHARP: return "A#";
			case DTROY::NOTE_B:       return "B";
			default: return "";
		}
	}
	
	string displayScale(int value) { 
		switch(value){
			case DTROY::AEOLIAN:        return "Aeolian";
			case DTROY::BLUES:          return "Blues";
			case DTROY::CHROMATIC:      return "Chromatic";
			case DTROY::DIATONIC_MINOR: return "Diatonic Minor";
			case DTROY::DORIAN:         return "Dorian";
			case DTROY::HARMONIC_MINOR: return "Harmonic Minor";
			case DTROY::INDIAN:         return "Indian";
			case DTROY::LOCRIAN:        return "Locrian";
			case DTROY::LYDIAN:         return "Lydian";
			case DTROY::MAJOR:          return "Major";
			case DTROY::MELODIC_MINOR:  return "Melodic Minor";
			case DTROY::MINOR:          return "Minor";
			case DTROY::MIXOLYDIAN:     return "Mixolydian";
			case DTROY::NATURAL_MINOR:  return "Natural Minor";
			case DTROY::PENTATONIC:     return "Pentatonic";
			case DTROY::PHRYGIAN:       return "Phrygian";
			case DTROY::TURKISH:        return "Turkish";
			case DTROY::NONE:           return "None";
			default: return "";
		}
	}
	
	string displayPlayMode(int value) { 
		switch(value){
			case 0: return "=>";
			case 1: return "<=";
			case 2: return "><";
			case 3: return "=*";
			case 4: return "=?";
			default: return "";
		}
	}
	

	void draw(NVGcontext *vg) override {
		if (++frame >= 4) {
			frame = 0;
			note = displayRootNote(module->rootNote);
			steps = "steps: " + to_string(module->numSteps());
			playMode = displayPlayMode(module->playMode);
			scale = displayScale(module->curScaleVal);
			//pattern = module->getPattern();
		}
		drawMessage(vg, Vec(0, 20), note, playMode, steps, scale, "");
	}
};

DTROYWidget::DTROYWidget() {
	DTROY *module = new DTROY();
	setModule(module);
	box.size = Vec(15*34, 380);

	{
		SVGPanel *panel = new SVGPanel();
		panel->box.size = box.size;
		panel->setBackground(SVG::load(assetPlugin(plugin, "res/DTROY.svg")));
		addChild(panel);
	}

	addChild(createScrew<ScrewSilver>(Vec(15, 0)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
	addChild(createScrew<ScrewSilver>(Vec(15, 365)));
	addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));
	
	{
		DTROYDisplay *display = new DTROYDisplay();
		display->module = module;
		display->box.pos = Vec(20, 253);
		display->box.size = Vec(250, 60);
		addChild(display);
	}
	
	addParam(createParam<RoundSmallBlackKnob>(Vec(18, 52), module, DTROY::CLOCK_PARAM, -2.0, 6.0, 2.0));
	addParam(createParam<LEDButton>(Vec(61, 56), module, DTROY::RUN_PARAM, 0.0, 1.0, 0.0));	
	addChild(createLight<SmallLight<GreenLight>>(Vec(67, 62), module, DTROY::RUNNING_LIGHT));	
	addParam(createParam<LEDButton>(Vec(99, 56), module, DTROY::RESET_PARAM, 0.0, 1.0, 0.0));	
	addChild(createLight<SmallLight<GreenLight>>(Vec(105, 62), module, DTROY::RESET_LIGHT));
	addParam(createParam<RoundSmallBlackSnapKnob>(Vec(133, 52), module, DTROY::STEPS_PARAM, 1.0, 16.0, 8.0));
	
	static const float portX0[4] = {20, 58, 96, 135};
 	addInput(createInput<PJ301MPort>(Vec(portX0[0], 90), module, DTROY::CLOCK_INPUT));
	addInput(createInput<PJ301MPort>(Vec(portX0[1], 90), module, DTROY::EXT_CLOCK_INPUT));
	addInput(createInput<PJ301MPort>(Vec(portX0[2], 90), module, DTROY::RESET_INPUT));
	addInput(createInput<PJ301MPort>(Vec(portX0[3], 90), module, DTROY::STEPS_INPUT));
	
	addParam(createParam<RoundSmallBlackKnob>(Vec(portX0[0]-1, 140), module, DTROY::ROOT_NOTE_PARAM, 0.0, DTROY::NUM_NOTES-1 + 0.1, 0));
	addParam(createParam<RoundSmallBlackKnob>(Vec(portX0[1]-1, 140), module, DTROY::SCALE_PARAM, 0.0, DTROY::NUM_SCALES-1 + 0.1, 0));
		
	addParam(createParam<RoundSmallBlackKnob>(Vec(portX0[2]-1, 140), module, DTROY::GATE_TIME_PARAM, 0.1, 1.0, 0.5));
	addParam(createParam<RoundSmallBlackKnob>(Vec(portX0[3]-1, 140), module, DTROY::SLIDE_TIME_PARAM	, 0.1, 1.0, 0.2));
	
	addInput(createInput<PJ301MPort>(Vec(portX0[0], 180), module, DTROY::ROOT_NOTE_INPUT));
	addInput(createInput<PJ301MPort>(Vec(portX0[1], 180), module, DTROY::SCALE_INPUT));
	addInput(createInput<PJ301MPort>(Vec(portX0[2], 180), module, DTROY::GATE_TIME_INPUT));
	addInput(createInput<PJ301MPort>(Vec(portX0[3], 180), module, DTROY::SLIDE_TIME_INPUT));
	
	addParam(createParam<CKD6>(Vec(portX0[0], 230), module, DTROY::PLAY_MODE_PARAM, 0.0, 4.0, 0.0));
	
	static const float portX1[8] = {200, 238, 276, 315, 353, 392, 430, 469};

	for (int i = 0; i < 8; i++) {
		addParam(createParam<RoundSmallBlackKnob>(Vec(portX1[i]-2, 52), module, DTROY::TRIG_PITCH_PARAM + i, 0.0, 10.0, 3.0));
		addParam(createParam<BidooSlidePotLong>(Vec(portX1[i]+2, 103), module, DTROY::TRIG_COUNT_PARAM + i, 1.0, 8.0, 1.0));
		addParam(createParam<BidooSlidePotShort>(Vec(portX1[i]+2, 220), module, DTROY::TRIG_TYPE_PARAM + i, 0.0, 5.0, 2.0));
		addParam(createParam<LEDButton>(Vec(portX1[i]+2, 313), module, DTROY::TRIG_SLIDE_PARAM + i, 0.0, 1.0, 0.0));
		addChild(createLight<SmallLight<GreenLight>>(Vec(portX1[i]+8, 319), module, DTROY::SLIDES_LIGHTS + i));	
		addParam(createParam<LEDButton>(Vec(portX1[i]+2, 338), module, DTROY::TRIG_SKIP_PARAM + i, 0.0, 1.0, 0.0));	
		addChild(createLight<SmallLight<GreenLight>>(Vec(portX1[i]+8, 344), module, DTROY::SKIPS_LIGHTS + i));
	}
	
	addInput(createInput<PJ301MPort>(Vec(portX0[0], 331), module, DTROY::EXTGATE1_INPUT));
	addInput(createInput<PJ301MPort>(Vec(portX0[1], 331), module, DTROY::EXTGATE2_INPUT));
	addOutput(createOutput<PJ301MPort>(Vec(portX0[2]-1, 331), module, DTROY::GATE_OUTPUT));
	addOutput(createOutput<PJ301MPort>(Vec(portX0[3]-1, 331), module, DTROY::PITCH_OUTPUT));
}
