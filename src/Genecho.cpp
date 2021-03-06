/*
 * GenEcho (Gendy / Grandy Echo module)
 * Samuel Laing - 2019
 *
 * VCV Rack module that uses granular stochastic methods to alter a sample
 */

#include "plugin.hpp"

#include "dsp/digital.hpp"
#include "dsp/resampler.hpp"

#include "wavetable.hpp"

#define MAX_BPTS 4096 
#define MAX_SAMPLE_SIZE 44100 

struct GenEcho : Module {
	enum ParamIds {
    BPTS_PARAM,
		TRIG_PARAM,
    GATE_PARAM, 
    ASTP_PARAM,
    DSTP_PARAM,
    ENVS_PARAM,
    SLEN_PARAM,
    BPTSCV_PARAM,
    ASTPCV_PARAM,
    DSTPCV_PARAM,
    MIRR_PARAM,
    PDST_PARAM,
    ACCM_PARAM,
    NUM_PARAMS
	};
	enum InputIds {
		WAV0_INPUT,
		GATE_INPUT,
    RSET_INPUT,
    BPTS_INPUT,
    ASTP_INPUT,
    DSTP_INPUT,
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

	float phase = 1.0;
	float blinkPhase = 0.0;

  dsp::SchmittTrigger smpTrigger;
  dsp::SchmittTrigger gTrigger;
  dsp::SchmittTrigger g2Trigger;
  dsp::SchmittTrigger resetTrigger;

  float sample[MAX_SAMPLE_SIZE] = {0.f};
  float _sample[MAX_SAMPLE_SIZE] = {0.f};

  unsigned int channels;
  unsigned int sampleRate;
 
  unsigned int sample_length = MAX_SAMPLE_SIZE;

  unsigned int idx = 0;

  // spacing between breakpoints... in samples rn
  unsigned int bpt_spc = 1500;
  unsigned int env_dur = bpt_spc / 2;

  // number of breakpoints - to be calculated according to size of
  // the sample
  unsigned int num_bpts = MAX_SAMPLE_SIZE / bpt_spc;

  float mAmps[MAX_BPTS] = {0.f};
  float mDurs[MAX_BPTS] = {1.f};

  Wavetable env = Wavetable(TRI); 

  unsigned int index = 0;
  
  float max_amp_step = 0.05f;
  float max_dur_step = 0.05f;

  float amp = 0.f; 
  float amp_next = 0.f;
  float g_idx = 0.f; 
  float g_idx_next = 0.5f;
 
  // when true read in from wav0_input and store in the sample buffer
  bool sampling = false;
  unsigned int s_i = 0;

  float bpts_sig = 1.f;
  float astp_sig = 1.f;
  float dstp_sig = 1.f;

  float astp = 1.f;
  float dstp = 1.f;

  bool is_mirroring = false;
  bool is_accumulating = false;
  
  gRandGen rg;
  DistType dt = LINEAR; 

  GenEcho() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    configParam(SLEN_PARAM, 0.01f, 1.f, 0.f);
    configParam(BPTS_PARAM, 0, 2200, 0.0);
    configParam(BPTSCV_PARAM, 0.f, 1.f, 0.0);
    configParam(ASTP_PARAM, 0.0, 0.6, 0.9);
    configParam(ASTPCV_PARAM, 0.f, 1.f, 0.f);
    configParam(DSTP_PARAM, 0.0, 0.2, 0.9);
    configParam(DSTPCV_PARAM, 0.f, 1.f, 0.f);
    configParam(ENVS_PARAM, 1.f, 4.f, 4.f);
    configParam(ACCM_PARAM, 0.f, 1.f, 0.f);
    configParam(MIRR_PARAM, 0.f, 1.f, 0.f);
    configParam(PDST_PARAM, 0.f, 2.f, 0.f);
  }

  void process(const ProcessArgs &args) override;
};

void GenEcho::process(const ProcessArgs &args) {
  // Implement a simple sine oscillator
  //float deltaTime = engineGetSampleTime();
  float amp_out = 0.0;

  // handle the 3 switches for accumlating and mirror toggle
  // and probability distrobution selection
  is_accumulating = (int) params[ACCM_PARAM].getValue();
  is_mirroring = (int) params[MIRR_PARAM].getValue();
  dt = (DistType) params[PDST_PARAM].getValue();

  // read in cv vals for astp, dstp and bpts
  bpts_sig = 5.f * dsp::quadraticBipolar((inputs[BPTS_INPUT].getVoltage() / 5.f) * params[BPTSCV_PARAM].getValue());
  astp_sig = dsp::quadraticBipolar((inputs[ASTP_INPUT].getVoltage() / 5.f) * params[ASTPCV_PARAM].getValue());
  dstp_sig = dsp::quadraticBipolar((inputs[DSTP_INPUT].getVoltage() / 5.f) * params[DSTPCV_PARAM].getValue());

  max_amp_step = rescale(params[ASTP_PARAM].getValue() + (astp_sig / 4.f), 0.0, 1.0, 0.05, 0.3);
  max_dur_step = rescale(params[DSTP_PARAM].getValue() + (dstp_sig / 4.f), 0.0, 1.0, 0.01, 0.3);

  sample_length = (int) (clamp(params[SLEN_PARAM].getValue(), 0.1, 1.f) * MAX_SAMPLE_SIZE);

  bpt_spc = (unsigned int) params[BPTS_PARAM].getValue() + 800;
  bpt_spc += (unsigned int) rescale(bpts_sig, -1.f, 1.f, 1.f, 200.f);
  num_bpts = sample_length / bpt_spc + 1;
 
  env_dur = bpt_spc / 2;

  // snap knob for selecting envelope for the grain
  int env_num = (int) clamp(roundf(params[ENVS_PARAM].getValue()), 1.0f, 4.0f);

  if (env.et != (EnvType) env_num) {
    env.switchEnvType((EnvType) env_num);
  }

  // handle sample reset
  if (smpTrigger.process(params[TRIG_PARAM].getValue()) || resetTrigger.process(inputs[RSET_INPUT].getVoltage() / 2.f)) {
    for (unsigned int i=0; i<MAX_SAMPLE_SIZE; i++) sample[i] = _sample[i];
    for (unsigned int i=0; i<MAX_BPTS; i++) {
      mAmps[i] = 0.f;
      mDurs[i] = 1.f; 
    }
  }

  // handle sample trigger through gate 
  if (g2Trigger.process(inputs[GATE_INPUT].getVoltage() / 2.f)) {

    // reset accumulated breakpoint vals
    for (unsigned int i=0; i<MAX_BPTS; i++) {
      mAmps[i] = 0.f;
      mDurs[i] = 1.f;
    }

    num_bpts = sample_length / bpt_spc;
    sampling = true;
    idx = 0;
    s_i = 0;
  }

  if (sampling) {
    if (s_i >= MAX_SAMPLE_SIZE - 50) {
      float x,y,p;
      x = sample[s_i-1];
      y = sample[0];
      p = 0.f;
      while (s_i < MAX_SAMPLE_SIZE) {
        sample[s_i] = (x * (1-p)) + (y * p);
        p += 1.f / 50.f;
        s_i++;
      }
      DEBUG("Finished sampling");
      sampling = false;
    } else {
      sample[s_i] = inputs[WAV0_INPUT].getVoltage(); 
      _sample[s_i] = sample[s_i];
      s_i++;
    } 
  }

  if (phase >= 1.0) {
    phase -= 1.0;

    amp = amp_next;
    index = (index + 1) % num_bpts;
    
    // adjust vals
    astp = max_amp_step * rg.my_rand(dt, random::normal());
    dstp = max_dur_step * rg.my_rand(dt, random::normal());

    if (is_mirroring) {
      mAmps[index] = mirror((is_accumulating ? mAmps[index] : 0.f) + astp, -1.0f, 1.0f); 
      mDurs[index] = mirror(mDurs[index] + (dstp), 0.5, 1.5);
    }
    else {
      mAmps[index] = wrap((is_accumulating ? mAmps[index] : 0.f) + astp, -1.0f, 1.0f); 
      mDurs[index] = wrap(mDurs[index] + dstp, 0.5, 1.5);
    }
  
    amp_next = mAmps[index];
    
    // step/adjust grain sample offsets 
    g_idx = g_idx_next;
    g_idx_next = 0.0;
  }

  // change amp in sample buffer
  sample[idx] = wrap(sample[idx] + (amp * env.get(g_idx)), -5.f, 5.f);
  amp_out = sample[idx];

  idx = (idx + 1) % sample_length;
  g_idx = fmod(g_idx + (1.f / (4.f * env_dur)), 1.f);
  g_idx_next = fmod(g_idx_next + (1.f / (4.f * env_dur)), 1.f);
  
  phase += 1.f / (mDurs[index] * bpt_spc);

  // get that amp OUT
  outputs[SINE_OUTPUT].setVoltage(amp_out);
}

struct GenEchoWidget : ModuleWidget {
	GenEchoWidget(GenEcho *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/GenEcho.svg")));

    addParam(createParam<RoundSmallBlackKnob>(Vec(9.883, 40.49), module, GenEcho::SLEN_PARAM));

    addParam(createParam<RoundSmallBlackKnob>(Vec(9.883, 139.97), module, GenEcho::BPTS_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(55.883, 168.88), module, GenEcho::BPTSCV_PARAM));
    
    addParam(createParam<RoundSmallBlackKnob>(Vec(9.883, 208.54), module, GenEcho::ASTP_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(55.883, 208.54), module, GenEcho::ASTPCV_PARAM));
    
    addParam(createParam<RoundSmallBlackKnob>(Vec(9.883, 277.11), module, GenEcho::DSTP_PARAM));
    addParam(createParam<RoundSmallBlackKnob>(Vec(55.883, 277.11), module, GenEcho::DSTPCV_PARAM));
    
    addParam(createParam<RoundBlackSnapKnob>(Vec(7.883, 344.25), module, GenEcho::ENVS_PARAM));

    // triggers for toggling mirroring and changing probability distribution
    addParam(createParam<CKSS>(Vec(60.789, 72.98), module, GenEcho::ACCM_PARAM)); 
    addParam(createParam<CKSS>(Vec(60.789, 103.69), module, GenEcho::MIRR_PARAM)); 
    addParam(createParam<CKSSThree>(Vec(60.789, 132.26), module, GenEcho::PDST_PARAM)); 
    
    addInput(createInput<PJ301MPort>(Vec(10.281, 69.79), module, GenEcho::WAV0_INPUT));
    addInput(createInput<PJ301MPort>(Vec(10.281, 95.54), module, GenEcho::GATE_INPUT));
    
    addInput(createInput<PJ301MPort>(Vec(58.281, 44.05), module, GenEcho::RSET_INPUT));
    
    addInput(createInput<PJ301MPort>(Vec(10.281, 169.01), module, GenEcho::BPTS_INPUT));
    addInput(createInput<PJ301MPort>(Vec(10.281, 236.72), module, GenEcho::ASTP_INPUT));
    addInput(createInput<PJ301MPort>(Vec(10.281, 306.00), module, GenEcho::DSTP_INPUT));

    addOutput(createOutput<PJ301MPort>(Vec(50.50, 347.46), module, GenEcho::SINE_OUTPUT));
  }
};

Model *modelGenEcho = createModel<GenEcho, GenEchoWidget>("GenEcho");
