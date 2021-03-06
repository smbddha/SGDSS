/*
 * Stitcher.cpp
 * Samuel Laing - 2019
 *
 * VCV Rack Stitcher module. An extened version of granular stochastic 
 * synthesis that connects waves produced by up to four seperate
 * GRANDY oscillators
 */

#include "plugin.hpp"
#include "dsp/digital.hpp"
#include "dsp/resampler.hpp"

#include "wavetable.hpp"
#include "GrandyOscillator.hpp"

#define NUM_OSCS 4

struct Stitcher : Module {
	enum ParamIds {
		G_FREQ_PARAM,
    G_ASTP_PARAM,
    G_DSTP_PARAM,
    G_BPTS_PARAM,
    G_GRAT_PARAM,
    G_FCAR_PARAM,
    G_FMOD_PARAM,
    G_IMOD_PARAM,
    G_FREQCV_PARAM,
    G_ASTPCV_PARAM,
    G_DSTPCV_PARAM,
    G_BPTSCV_PARAM,
    G_GRATCV_PARAM,
    G_FCARCV_PARAM,
    G_FMODCV_PARAM,
    G_IMODCV_PARAM,
    G_NOSC_PARAM,
    TRIG_PARAM,
    ENUMS(F_PARAM, NUM_OSCS),
    ENUMS(B_PARAM, NUM_OSCS),
    ENUMS(A_PARAM, NUM_OSCS),
    ENUMS(D_PARAM, NUM_OSCS),
    ENUMS(G_PARAM, NUM_OSCS),
    ENUMS(FCAR_PARAM, NUM_OSCS),
    ENUMS(FMOD_PARAM, NUM_OSCS),
    ENUMS(IMOD_PARAM, NUM_OSCS),
    ENUMS(FCARCV_PARAM, NUM_OSCS),
    ENUMS(FMODCV_PARAM, NUM_OSCS),
    ENUMS(IMODCV_PARAM, NUM_OSCS),
    ENUMS(FCV_PARAM, NUM_OSCS),
    ENUMS(BCV_PARAM, NUM_OSCS),
    ENUMS(ACV_PARAM, NUM_OSCS),
    ENUMS(DCV_PARAM, NUM_OSCS),
    ENUMS(GCV_PARAM, NUM_OSCS),
    ENUMS(ST_PARAM, NUM_OSCS),
    FMTR_PARAM,
    PDST_PARAM,
    MIRR_PARAM,
    NUM_PARAMS
	};
	enum InputIds {
		WAV0_INPUT,
    G_FREQ_INPUT,
    G_ASTP_INPUT,
    G_DSTP_INPUT,
    G_BPTS_INPUT,
    G_GRAT_INPUT,
    G_FCAR_INPUT,
    G_FMOD_INPUT,
    G_IMOD_INPUT,
		ENUMS(F_INPUT, NUM_OSCS),
    ENUMS(B_INPUT, NUM_OSCS),
    ENUMS(A_INPUT, NUM_OSCS),
    ENUMS(D_INPUT, NUM_OSCS),
    ENUMS(G_INPUT, NUM_OSCS),
    ENUMS(FCAR_INPUT, NUM_OSCS),
    ENUMS(FMOD_INPUT, NUM_OSCS),
    ENUMS(IMOD_INPUT, NUM_OSCS),
    NUM_INPUTS
	};
	enum OutputIds {
		SINE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT,
    ENUMS(ONOFF_LIGHT, NUM_OSCS),
		NUM_LIGHTS
	};

	float blinkPhase = 0.0;

  dsp::SchmittTrigger smpTrigger;
  
  GendyOscillator gos[NUM_OSCS];
  int osc_idx = 0;

  // allow an adjustable number of oscillators
  // to be used 1 -> 4
  int curr_num_oscs = NUM_OSCS;
  int stutters[NUM_OSCS] = {1};
  int current_stutter = 1;

  float phase = 0.f;
  float amp = 0.f;
  float amp_next = 0.f;
  float amp_out = 0.f;
  float speed = 0.f;

  bool is_swapping = false;
  int stutter = 1;

  // vars for global parameter controls
  float g_freq_mul = 1.0;
  float g_max_amp_add = 0.f;
  float g_max_dur_add = 0.f;

  float g_freq_sig = 0.f;
  float g_bpts_sig = 0.f;
  float g_astp_sig = 0.f;
  float g_dstp_sig = 0.f;
  float g_grat_sig = 0.f;
  float g_fcar_sig = 0.f;
  float g_fmod_sig = 0.f;
  float g_imod_sig = 0.f;

  float freq_sig = 0.f;
  float bpts_sig = 0.f;
  float astp_sig = 0.f;
  float dstp_sig = 0.f;
  float grat_sig = 0.f;
  float fcar_sig = 0.f;
  float fmod_sig = 0.f;
  float imod_sig = 0.f;

  bool g_is_mirroring = false;
  bool g_is_fm_on = false;
  DistType g_dt = LINEAR;

  Stitcher() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    for (int i = 0; i < NUM_OSCS; i++) {
      configParam(F_PARAM + i, -4.f, 4.f, 0.f);
      configParam(B_PARAM + i, 3.f, MAX_BPTS, 0.f);
      configParam(A_PARAM + i, 0.f, 1.f, 0.f);
      configParam(D_PARAM + i, 0.f, 1.f, 0.f);
      configParam(G_PARAM + i, 0.7, 1.3, 0.0);
      configParam(FCV_PARAM + i, 0.f, 1.f, 0.f);
      configParam(BCV_PARAM + i, 0.f, 1.f, 0.f);
      configParam(ACV_PARAM + i, 0.f, 1.f, 0.f);
      configParam(DCV_PARAM + i, 0.f, 1.f, 0.f);
      configParam(GCV_PARAM + i, 0.f, 1.f, 0.f);
      configParam(ST_PARAM + i, 1.f, 5.f, 5.f);
    }

    configParam(G_FREQ_PARAM, -1.f, 1.f, 0.f);
    configParam(G_BPTS_PARAM, -1.f, 1.f, 0.f);
    configParam(G_ASTP_PARAM, -1.f, 1.f, 0.f);
    configParam(G_DSTP_PARAM, -1.f, 1.f, 0.f);
    configParam(G_GRAT_PARAM, -1.f, 1.f, 0.f);
    configParam(G_FCAR_PARAM, -1.f, 1.f, 0.f);
    configParam(G_FMOD_PARAM, -1.f, 1.f, 0.f);
    configParam(G_IMOD_PARAM, -1.f, 1.f, 0.f);
    configParam(G_FREQCV_PARAM, 0.f, 1.f, 0.f);
    configParam(G_BPTSCV_PARAM, 0.f, 1.f, 0.f);
    configParam(G_ASTPCV_PARAM, 0.f, 1.f, 0.f);
    configParam(G_DSTPCV_PARAM, 0.f, 1.f, 0.f);
    configParam(G_GRATCV_PARAM, 0.f, 1.f, 0.f);
    configParam(G_FCARCV_PARAM, 0.f, 1.f, 0.f);
    configParam(G_FMODCV_PARAM, 0.f, 1.f, 0.f);
    configParam(G_IMODCV_PARAM, 0.f, 1.f, 0.f);
    configParam(G_NOSC_PARAM, 1.f, 4.f, 4.f);
    configParam(FMTR_PARAM, 0.0f, 1.0f, 0.0f);
    configParam(MIRR_PARAM, 0.f, 1.f, 0.f);
    configParam(PDST_PARAM, 0.f, 2.f, 0.f);
  }

  void process(const ProcessArgs &args) override;
  float wrap(float,float,float);
};

void Stitcher::process(const ProcessArgs &args) {
  float deltaTime = args.sampleTime;

  // read in global switches
  g_is_mirroring = (int) params[MIRR_PARAM].getValue();
  g_is_fm_on = !(params[FMTR_PARAM].getValue() > 0.f); 
  g_dt = (DistType) params[PDST_PARAM].getValue();
  
  // read in global controls
  g_freq_sig = params[G_FREQ_PARAM].getValue();
  g_bpts_sig = params[G_BPTS_PARAM].getValue();
  g_astp_sig = params[G_ASTP_PARAM].getValue();
  g_dstp_sig = params[G_DSTP_PARAM].getValue();
  g_grat_sig = params[G_GRAT_PARAM].getValue();
  
  g_fcar_sig = params[G_FCAR_PARAM].getValue();
  g_fmod_sig = params[G_FMOD_PARAM].getValue();
  g_imod_sig = params[G_IMOD_PARAM].getValue();

  g_freq_sig = (inputs[G_FREQ_INPUT].getVoltage() / 5.f) * params[G_FREQCV_PARAM].getValue();
  g_bpts_sig = (inputs[G_BPTS_INPUT].getVoltage() / 5.f) * params[G_BPTSCV_PARAM].getValue();
  g_astp_sig = (inputs[G_ASTP_INPUT].getVoltage() / 5.f) * params[G_ASTPCV_PARAM].getValue();
  g_dstp_sig = (inputs[G_DSTP_INPUT].getVoltage() / 5.f) * params[G_DSTPCV_PARAM].getValue();
  g_grat_sig = (inputs[G_GRAT_INPUT].getVoltage() / 5.f) * params[G_GRATCV_PARAM].getValue();

  g_fcar_sig += (inputs[G_FCAR_INPUT].getVoltage() / 5.f) * params[G_FCARCV_PARAM].getValue();
  g_fmod_sig += (inputs[G_FMOD_INPUT].getVoltage() / 5.f) * params[G_FMODCV_PARAM].getValue();
  g_imod_sig += (inputs[G_IMOD_INPUT].getVoltage() / 5.f) * params[G_IMODCV_PARAM].getValue();

  int prev = curr_num_oscs;
  curr_num_oscs = (int) clamp(params[G_NOSC_PARAM].getValue(), 1.f, 4.f);

  if (prev != curr_num_oscs) DEBUG("new # of oscs: %d\n", curr_num_oscs);

  // read in all the parameters for each oscillator
  for (int i=0; i<NUM_OSCS; i++) {
	
    lights[ONOFF_LIGHT + i].setBrightness(i < curr_num_oscs ? 1.0f : 0.0f);
    stutters[i] = (int) params[ST_PARAM + i].getValue();
    
    gos[i].is_mirroring = g_is_mirroring;
    gos[i].is_fm_on = g_is_fm_on;
    gos[i].dt = g_dt;

    // accept modulation of signal inputs for each parameter
        
    freq_sig = (inputs[F_INPUT + i].getVoltage() / 5.f) * params[FCV_PARAM + i].getValue();
    freq_sig += g_freq_sig;
    freq_sig += params[F_PARAM + i].getValue();
    gos[i].freq = clamp(261.626f * powf(2.0f, freq_sig), 1.f, 3000.f);

    bpts_sig = 5.f * dsp::quadraticBipolar((inputs[B_INPUT + i].getVoltage() / 5.f) * params[BCV_PARAM + i].getValue());
    bpts_sig += g_bpts_sig;
    gos[i].num_bpts = clamp((int) params[B_PARAM + i].getValue() + (int) bpts_sig, 2, MAX_BPTS);
    
    astp_sig = dsp::quadraticBipolar((inputs[A_INPUT + i].getVoltage() / 5.f) * params[ACV_PARAM + i].getValue());
    astp_sig += g_astp_sig;
    gos[i].max_amp_step = rescale(params[A_PARAM + i].getValue() + (astp_sig / 4.f), 0.0, 1.0, 0.05, 0.3);
    
    dstp_sig = dsp::quadraticBipolar((inputs[D_INPUT + i].getVoltage() / 5.f) * params[DCV_PARAM].getValue());
    dstp_sig += g_dstp_sig;
    gos[i].max_dur_step = rescale(params[D_PARAM + i].getValue() + (dstp_sig / 4.f), 0.0, 1.0, 0.01, 0.3);

    grat_sig = (inputs[G_INPUT + i].getVoltage() / 5.f) * params[GCV_PARAM].getValue();
    gos[i].g_rate = clamp(261.626f * powf(2.0f, grat_sig + g_grat_sig), 1e-6, 3000.f);
    
    // fm control sigs
    fcar_sig = g_grat_sig;
    fcar_sig += g_fcar_sig;
    fcar_sig += params[FCAR_PARAM + i].getValue();
    gos[i].f_car = clamp(261.626f * powf(2.0f, fcar_sig), 1.f, 3000.f);
  
    // no local controls for the frequency of the modulating signal, so just 
    // respond to the global control values
    gos[i].f_mod = clamp(261.626f * powf(2.0f, g_fmod_sig), 1.f, 3000.f);
  
    imod_sig = dsp::quadraticBipolar((inputs[IMOD_INPUT + i].getVoltage() / 5.f) * params[IMODCV_PARAM + i].getValue());
    imod_sig += g_imod_sig; 
    imod_sig += params[IMOD_PARAM].getValue();
    gos[i].i_mod = rescale(imod_sig, 0.f, 1.f, 10.f, 3000.f);
  }

  if (is_swapping) {
    amp_out = ((1.0 - phase) * amp) + (phase * amp_next); 
    phase += speed;
    
    if (phase >= 1.0) is_swapping = false;
  } else {
    
    gos[osc_idx].process(deltaTime);
    amp_out = gos[osc_idx].out();
    
    if (gos[osc_idx].last_flag) {
      current_stutter--;
      if (current_stutter < 1) {
        amp = amp_out;
        speed = gos[osc_idx].speed;
        osc_idx = (osc_idx + 1) % curr_num_oscs;
        

        gos[osc_idx].process(deltaTime);
        amp_next = gos[osc_idx].out();  
       
        current_stutter = stutters[osc_idx];

        phase = 0.f;
        is_swapping = true;
      }
    }
  }
  
  outputs[SINE_OUTPUT].setVoltage(5.0f * amp_out);
}

struct StitcherWidget : ModuleWidget {
	StitcherWidget(Stitcher *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Stitch.svg")));

    /*
    // no screws ... at least for now
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 6 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    */

    for (int i = 0; i < NUM_OSCS; i++) {
      addParam(createParam<RoundSmallBlackKnob>(Vec(10.004, 15.89+(i*95)), module, Stitcher::F_PARAM + i));
      addParam(createParam<RoundSmallBlackKnob>(Vec(36.004, 15.89+(i*95)), module, Stitcher::B_PARAM + i));
      
      addParam(createParam<RoundSmallBlackKnob>(Vec(62.004, 15.89+(i*95)), module, Stitcher::A_PARAM + i));
      addParam(createParam<RoundSmallBlackKnob>(Vec(88.004, 15.89+(i*95)), module, Stitcher::D_PARAM + i));
    
      addParam(createParam<RoundSmallBlackKnob>(Vec(114.004, 15.89+(i*95)), module, Stitcher::G_PARAM + i));
      
      addParam(createParam<RoundSmallBlackKnob>(Vec(10.004, 41.89+(i*95)), module, Stitcher::FCV_PARAM + i));
      addParam(createParam<RoundSmallBlackKnob>(Vec(36.004, 41.89+(i*95)), module, Stitcher::BCV_PARAM + i));
      
      addParam(createParam<RoundSmallBlackKnob>(Vec(62.004, 41.89+(i*95)), module, Stitcher::ACV_PARAM + i));
      addParam(createParam<RoundSmallBlackKnob>(Vec(88.004, 41.89+(i*95)), module, Stitcher::DCV_PARAM + i));
    
      addParam(createParam<RoundSmallBlackKnob>(Vec(114.004, 41.89+(i*95)), module, Stitcher::GCV_PARAM + i));
      
      // stutter param
      addParam(createParam<RoundBlackSnapKnob>(Vec(149.640, 28.57+(i*95)), module, Stitcher::ST_PARAM + i));

      // CV inputs
      addInput(createInput<PJ301MPort>(Vec(10.004, 69.39+(i*95)), module, Stitcher::F_INPUT + i));
      addInput(createInput<PJ301MPort>(Vec(36.004, 69.39+(i*95)), module, Stitcher::B_INPUT + i));
      
      addInput(createInput<PJ301MPort>(Vec(62.004, 69.39+(i*95)), module, Stitcher::A_INPUT + i));
      addInput(createInput<PJ301MPort>(Vec(88.004, 69.39+(i*95)), module, Stitcher::D_INPUT + i));
      
      addInput(createInput<PJ301MPort>(Vec(114.004, 69.39+(i*95)), module, Stitcher::G_INPUT + i));

      // light to signal if oscillator on / off 
		  addChild(createLight<SmallLight<GreenLight>>(Vec(149.185, 80+(i*95)), module, Stitcher::ONOFF_LIGHT + i));
    }

    // global controls (on the right of the panel)
    addParam(createParam<RoundSmallBlackKnob>(Vec(231.140, 31.77), module, Stitcher::G_FREQ_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(231.140, 65.77), module, Stitcher::G_BPTS_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(231.140, 99.77), module, Stitcher::G_ASTP_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(231.140, 133.77), module, Stitcher::G_DSTP_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(231.140, 166.77), module, Stitcher::G_GRAT_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(231.140, 205.77), module, Stitcher::G_FCAR_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(231.140, 239.77), module, Stitcher::G_FMOD_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(231.140, 273.77), module, Stitcher::G_IMOD_PARAM));
    
    addParam(createParam<RoundSmallBlackKnob>(Vec(263.140, 31.77), module, Stitcher::G_FREQCV_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(263.140, 65.77), module, Stitcher::G_BPTSCV_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(263.140, 99.77), module, Stitcher::G_ASTPCV_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(263.140, 133.77), module, Stitcher::G_DSTPCV_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(263.140, 166.77), module, Stitcher::G_GRATCV_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(263.140, 205.77), module, Stitcher::G_FCARCV_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(263.140, 239.77), module, Stitcher::G_FMODCV_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(263.140, 273.77), module, Stitcher::G_IMODCV_PARAM));

    addInput(createInput<PJ301MPort>(Vec(293.539, 31.77), module, Stitcher::G_FREQ_INPUT));
    addInput(createInput<PJ301MPort>(Vec(293.539, 65.77), module, Stitcher::G_BPTS_INPUT));
    addInput(createInput<PJ301MPort>(Vec(293.539, 99.77), module, Stitcher::G_ASTP_INPUT));
    addInput(createInput<PJ301MPort>(Vec(293.539, 133.77), module, Stitcher::G_DSTP_INPUT));
    addInput(createInput<PJ301MPort>(Vec(293.539, 166.77), module, Stitcher::G_GRAT_INPUT));
    addInput(createInput<PJ301MPort>(Vec(293.539, 205.77), module, Stitcher::G_FCAR_INPUT));
    addInput(createInput<PJ301MPort>(Vec(293.539, 239.77), module, Stitcher::G_FMOD_INPUT));
    addInput(createInput<PJ301MPort>(Vec(293.539, 273.77), module, Stitcher::G_IMOD_INPUT));

    addParam(createParam<RoundBlackSnapKnob>(Vec(277.140, 311.80), module, Stitcher::G_NOSC_PARAM));

    // the few switches for fm toggle, probability distrobution selection 
    // and mirroring toggle
    addParam(createParam<CKSS>(Vec(210.392, 309.22), module, Stitcher::FMTR_PARAM));
    addParam(createParam<CKSS>(Vec(244.392, 329.22), module, Stitcher::MIRR_PARAM)); 
    addParam(createParam<CKSSThree>(Vec(210.392, 343.16), module, Stitcher::PDST_PARAM)); 
		
    addOutput(createOutput<PJ301MPort>(Vec(278.140, 347.50), module, Stitcher::SINE_OUTPUT));
  }
};

Model *modelStitcher = createModel<Stitcher, StitcherWidget>("Stitcher");
