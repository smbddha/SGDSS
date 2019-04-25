/*
 * MyModule.cpp
 * Samuel Laing - 2019
 *
 * VCV Rack Module with a single GRANDY oscillator
 *
 */

#include "Gendy.hpp"

#include "GendyOscillator.hpp"

struct MyModule : Module {
	enum ParamIds {
		FREQ_PARAM,
    ASTP_PARAM,
    DSTP_PARAM,
    BPTS_PARAM,
    GRAT_PARAM,
    FREQCV_PARAM,
    ASTPCV_PARAM,
    DSTPCV_PARAM,
    BPTSCV_PARAM,
    TRIG_PARAM,
    FMTR_PARAM,
		ENVS_PARAM,
    FMOD_PARAM,
    FCAR_PARAM,
    IMOD_PARAM,
    NUM_PARAMS
	};
	enum InputIds {
		FREQ_INPUT,
    ASTP_INPUT,
    DSTP_INPUT,
    BPTS_INPUT,
    GRAT_INPUT,
    ENVS_INPUT,
    NUM_INPUTS
	};
	enum OutputIds {
		SINE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT,
		NUM_LIGHTS
	};

	//float phase = 1.0;
	float blinkPhase = 0.0;

  SchmittTrigger smpTrigger;
  
  GendyOscillator go;

  EnvType env = (EnvType) 1;

  float freq_sig = 1.f;
  float astp_sig = 1.f;
  float dstp_sig = 1.f;
  float grat_sig = 1.f;
  float envs_sig = 1.f;

  bool fm_is_on = false;

  // For more advanced Module features, read Rack's engine.hpp header file
	// - toJson, fromJson: serialization of internal data
	// - onSampleRateChange: event triggered by a change of sample rate
	// - onReset, onRandomize, onCreate, onDelete: implements special behavior when user clicks these from the context menu

  MyModule() : Module(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {
  }
	
  void step() override;
  float wrap(float,float,float);
};

void MyModule::step() {
  float deltaTime = engineGetSampleTime();

  int new_nbpts = clamp((int) params[BPTS_PARAM].value, 3, MAX_BPTS);
  if (new_nbpts != go.num_bpts) go.num_bpts = new_nbpts;

  //int env_next = (int) params[ENVS_PARAM].value + 1;
  
  // snap knob for selecting envelope for the grain
  int env_num = (int) clamp(roundf(params[ENVS_PARAM].value), 1.0f, 8.0f);

  if (env != (EnvType) env_num) {
    debug("Switching to env type: %d", env_num);
    env = (EnvType) env_num;
    go.env.switchEnvType(env);
  }

  //if (phase >= 1.0) debug("PITCH PARAM: %f\n", (float) params[PITCH_PARAM].value);
  //TODO
  // add mirroring function to complement the wrap
  // and then add switch to change between the two
 
  // TODO
  // accept modulation of signal inputs for each parameter
  freq_sig = inputs[FREQ_INPUT].value;
  astp_sig = inputs[ASTP_INPUT].value;
  dstp_sig = inputs[DSTP_INPUT].value;
  grat_sig = inputs[GRAT_INPUT].value;
  envs_sig = inputs[ENVS_INPUT].value;
  
  go.max_amp_step = rescale(params[ASTP_PARAM].value, 0.0, 1.0, 0.05, 0.3);
  go.max_dur_step = rescale(params[DSTP_PARAM].value, 0.0, 1.0, 0.01, 0.3);
  go.freq_mul = rescale(params[FREQ_PARAM].value, -1.0, 1.0, 0.05, 4.0);
  go.g_rate = rescale(params[GRAT_PARAM].value, 0.f, 1.f, 0.5f, 8.0f);

  // set fm params
  go.is_fm_on = !(params[FMTR_PARAM].value > 0.0f);
  go.f_car = rescale(params[FCAR_PARAM].value, 0.f, 1.f, 5.f, 3000.f);
  go.f_mod = rescale(params[FMOD_PARAM].value, 0.f, 1.f, 5.f, 3000.f);
  go.i_mod = rescale(params[IMOD_PARAM].value, 0.f, 1.f, 10.f, 3000.f);

  go.process(deltaTime);

  outputs[SINE_OUTPUT].value = 5.0f * go.out();
}


struct MyModuleWidget : ModuleWidget {
	MyModuleWidget(MyModule *module) : ModuleWidget(module) {
		setPanel(SVG::load(assetPlugin(plugin, "res/MyModule3.svg")));

		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    //addParam(ParamWidget::create<CKSSThree>(Vec(110, 240), module, MyModule::ENVS_PARAM, 0.0f, 2.0f, 0.0f));
	
    // knob params
    addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(22.757, 47.61), module, MyModule::FREQ_PARAM, -1.0, 1.0, 0.0));
    addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(53.360, 97.90), module, MyModule::FREQCV_PARAM, 0.f, 1.f, 0.f));
    
    addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(110.757, 47.61), module, MyModule::BPTS_PARAM, 3, MAX_BPTS, 0));
    addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(141.360, 97.90), module, MyModule::BPTSCV_PARAM, 3, MAX_BPTS, 0));
    
    addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(22.757, 141.61), module, MyModule::DSTP_PARAM, 0.0, 1.0, 0.9));
    addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(53.360, 191.95), module, MyModule::DSTPCV_PARAM, 0.0, 1.0, 0.9));
    
    addParam(ParamWidget::create<RoundLargeBlackKnob>(Vec(110.757, 141.61), module, MyModule::ASTP_PARAM, 0.0, 1.0, 0.9));
    addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(141.360, 191.95), module, MyModule::ASTPCV_PARAM, 0.0, 1.0, 0.9));
    
    addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(35.360, 241.85), module, MyModule::GRAT_PARAM, 0.f, 1.f, 0.0));
    
    addParam(ParamWidget::create<RoundBlackSnapKnob>(Vec(133.360, 241.85), module, MyModule::ENVS_PARAM, 1.0f, 4.0f, 4.0f));

    // for fm 
    addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(9.360, 299.79), module, MyModule::FCAR_PARAM, 0.f, 1.f, 0.f));
    addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(49.360, 299.79), module, MyModule::FMOD_PARAM, 0.f, 1.f, 0.f));
    addParam(ParamWidget::create<RoundSmallBlackKnob>(Vec(29.260, 344.68), module, MyModule::IMOD_PARAM, 0.f, 1.f, 0.f));

	  addParam(ParamWidget::create<CKSS>(Vec(12.094, 264.98), module, MyModule::FMTR_PARAM, 0.0f, 1.0f, 0.0f));
    
    // signal inputs
    addInput(Port::create<PJ301MPort>(Vec(15.73, 99.81), Port::INPUT, module, MyModule::FREQ_INPUT));
    addInput(Port::create<PJ301MPort>(Vec(104.02, 99.81), Port::INPUT, module, MyModule::BPTS_INPUT));
    
    addInput(Port::create<PJ301MPort>(Vec(15.73, 193.48), Port::INPUT, module, MyModule::ASTP_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(104.02, 193.48), Port::INPUT, module, MyModule::DSTP_INPUT));
    
    addInput(Port::create<PJ301MPort>(Vec(100, 340.42), Port::INPUT, module, MyModule::ENVS_INPUT));
		addInput(Port::create<PJ301MPort>(Vec(100, 285.33), Port::INPUT, module, MyModule::GRAT_INPUT));

    // output signal 
    addOutput(Port::create<PJ301MPort>(Vec(134.003, 334.86), Port::OUTPUT, module, MyModule::SINE_OUTPUT));

		//addChild(ModuleLightWidget::create<MediumLight<RedLight>>(Vec(41, 59), module, MyModule::BLINK_LIGHT));
	}
};


// Specify the Module and ModuleWidget subclass, human-readable
// author name for categorization per plugin, module slug (should never
// change), human-readable module name, and any number of tags
// (found in `include/tags.hpp`) separated by commas.
Model *modelMyModule = Model::create<MyModule, MyModuleWidget>("Gendy", "MyModule", "My Module", OSCILLATOR_TAG);
