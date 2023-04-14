// Venom Modules (c) 2022 Dave Benham
// Licensed under GNU GPLv3

#include "plugin.hpp"
#include "dsp/math.hpp"
#include "OversampleFilter.hpp"

#define MODULE_NAME Mix4

struct Mix4 : VenomModule {
  enum ParamId {
    ENUMS(LEVEL_PARAMS, 4),
    MIX_LEVEL_PARAM,
    MODE_PARAM,
    CLIP_PARAM,
    DCBLOCK_PARAM,
    PARAMS_LEN
  };
  enum InputId {
    ENUMS(INPUTS, 4),
    INPUTS_LEN
  };
  enum OutputId {
    MIX_OUTPUT,
    OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  int mode = -1;
  bool connected[4] = {false, false, false, false};
  float normal = 0.f;
  float scale = 1.f;
  float offset = 0.f;
  int oversample = 4;
  OversampleFilter_4 outUpSample[4], outDownSample[4];
  DCBlockFilter_4 dcBlockBeforeFilter[4], dcBlockAfterFilter[4];

  Mix4() {
    struct FixedSwitchQuantity : SwitchQuantity {
      std::string getDisplayValueString() override {
        return labels[getValue()];
      }
    };
    venomConfig(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    for (int i=0; i < 4; i++){
      configParam(LEVEL_PARAMS+i, 0.f, 2.f, 1.f, string::f("Channel %d level", i + 1), " dB", -10.f, 20.f);
      configInput(INPUTS+i, string::f("Channel %d", i + 1));
    }
    configParam(MIX_LEVEL_PARAM, 0.f, 2.f, 1.f, "Mix level", " dB", -10.f, 20.f);
    configSwitch<FixedSwitchQuantity>(MODE_PARAM, 0.f, 4.f, 0.f, "Level Mode", {"Unipolar audio dB", "Unipolar audio dB poly sum", "Bipolar CV%", "Bipolar CV x2", "Bipolar CV x10"});
    configSwitch<FixedSwitchQuantity>(DCBLOCK_PARAM, 0.f, 3.f, 0.f, "DC Block", {"Off", "Before clipping", "Before and after clipping", "After clipping"});
    configSwitch<FixedSwitchQuantity>(CLIP_PARAM, 0.f, 3.f, 0.f, "Clipping", {"Off", "Hard CV clipping", "Soft audio clipping", "Soft oversampled audio clipping"});
    configOutput(MIX_OUTPUT, "Mix");
    initOversample();
    initDCBlock();
  }

  void initOversample(){
    for (int i=0; i<4; i++){
      outUpSample[i].setOversample(oversample);
      outDownSample[i].setOversample(oversample);
    }
  }

  void initDCBlock(){
    float sampleTime = settings::sampleRate;
    for (int i=0; i<4; i++){
      dcBlockBeforeFilter[i].init(sampleTime);
      dcBlockAfterFilter[i].init(sampleTime);
    }
  }

  void onReset(const ResetEvent& e) override {
    mode = -1;
    initOversample();
    Module::onReset(e);
  }
  
  void onSampleRateChange(const SampleRateChangeEvent& e) override {
    initDCBlock();
  }

  void process(const ProcessArgs& args) override {
    VenomModule::process(args);
    if( static_cast<int>(params[MODE_PARAM].getValue()) != mode ||
        connected[0] != inputs[INPUTS + 0].isConnected() ||
        connected[1] != inputs[INPUTS + 1].isConnected() ||
        connected[2] != inputs[INPUTS + 2].isConnected() ||
        connected[3] != inputs[INPUTS + 3].isConnected()
    ){
      mode = static_cast<int>(params[MODE_PARAM].getValue());
      ParamQuantity* q;
      for (int i=0; i<4; i++) {
        connected[i] = inputs[INPUTS + i].isConnected();
        q = paramQuantities[LEVEL_PARAMS + i];
        q->unit = mode <= 1 ? " dB" : !connected[i] ? " V" : mode == 2 ? "%" : "x";
        q->displayBase = mode <= 1 ? -10.f : 0.f;
        q->displayMultiplier = mode <= 1 ? 20.f : (mode == 2 && connected[i]) ? 100.f : (mode == 3 && connected[i]) ? 2.f : 10.f;
        q->displayOffset = mode <= 1 ? 0.f : (mode == 2 && connected[i]) ? -100.f : (mode == 3 && connected[i]) ? -2.f : -10.f;
      }
      q = paramQuantities[MIX_LEVEL_PARAM];
      q->unit = mode <= 1 ? " dB" : mode == 2 ? "%" : "x";
      q->displayBase = mode <= 1 ? -10.f : 0.f;
      q->displayMultiplier = mode <= 1 ? 20.f : mode == 2 ? 100.f : mode == 3 ? 2.f : 10.f;
      q->displayOffset = mode <= 1 ? 0.f : mode == 2 ? -100.f : mode == 3 ? -2.f : -10.f;
      q->defaultValue = mode <= 1 ? 1.f : mode == 2 ? 2.f : mode == 3 ? 1.5f : 1.1f;
      normal = mode <= 1 ? 0.f : mode == 2 ? 10.f : mode == 3 ? 5.f : 1.f;
      scale = mode == 4 ? 10.f : mode == 3 ? 2.f : 1.f;
      offset = mode <= 1 ? 0.f : -1.f;
    }
    int clip = static_cast<int>(params[CLIP_PARAM].getValue());
    int dcBlock = static_cast<int>(params[DCBLOCK_PARAM].getValue());

    int channels = mode == 1 ? 1 : std::max({1, inputs[INPUTS].getChannels(), inputs[INPUTS+1].getChannels(), inputs[INPUTS+2].getChannels(), inputs[INPUTS+3].getChannels()});
    simd::float_4 out;
    for (int c=0; c<channels; c+=4){
      out = mode == 1 ?
            inputs[INPUTS+0].getVoltageSum() * (params[LEVEL_PARAMS+0].getValue()+offset)*scale
          + inputs[INPUTS+1].getVoltageSum() * (params[LEVEL_PARAMS+1].getValue()+offset)*scale
          + inputs[INPUTS+2].getVoltageSum() * (params[LEVEL_PARAMS+2].getValue()+offset)*scale
          + inputs[INPUTS+3].getVoltageSum() * (params[LEVEL_PARAMS+3].getValue()+offset)*scale
        :
            inputs[INPUTS+0].getNormalPolyVoltageSimd<simd::float_4>(normal, c) * (params[LEVEL_PARAMS+0].getValue()+offset)*scale
          + inputs[INPUTS+1].getNormalPolyVoltageSimd<simd::float_4>(normal, c) * (params[LEVEL_PARAMS+1].getValue()+offset)*scale
          + inputs[INPUTS+2].getNormalPolyVoltageSimd<simd::float_4>(normal, c) * (params[LEVEL_PARAMS+2].getValue()+offset)*scale
          + inputs[INPUTS+3].getNormalPolyVoltageSimd<simd::float_4>(normal, c) * (params[LEVEL_PARAMS+3].getValue()+offset)*scale;
      out *= (params[MIX_LEVEL_PARAM].getValue()+offset)*scale;
      if (dcBlock && dcBlock <= 2)
        out = dcBlockBeforeFilter[c/4].process(out);
      if (clip == 1)
        out = clamp(out, -10.f, 10.f);
      if (clip == 2)
        out = softClip(out);
      if (clip == 3){
        for (int i=0; i<oversample; i++){
          out = outUpSample[c/4].process(i ? simd::float_4::zero() : out*oversample);
          out = softClip(out);
          out = outDownSample[c/4].process(out);
        }
      }
      if (dcBlock == 3 || (dcBlock == 2 && clip))
        out = dcBlockAfterFilter[c/4].process(out);
      outputs[MIX_OUTPUT].setVoltageSimd(out, c);
    }
    outputs[MIX_OUTPUT].setChannels(channels);
  }

};

struct Mix4Widget : VenomWidget {

  struct ModeSwitch : GlowingSvgSwitchLockable {
    ModeSwitch() {
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallPinkButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallPurpleButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallGreenButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallLightBlueButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallBlueButtonSwitch.svg")));
    }
  };

  struct ClipSwitch : GlowingSvgSwitchLockable {
    ClipSwitch() {
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallOffButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallWhiteButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallYellowButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallOrangeButtonSwitch.svg")));
    }
  };

  struct DCBlockSwitch : GlowingSvgSwitchLockable {
    DCBlockSwitch() {
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallOffButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallYellowButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallGreenButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallLightBlueButtonSwitch.svg")));
    }
  };

  Mix4Widget(Mix4* module) {
    setModule(module);
    setVenomPanel("Mix4");

    addParam(createLockableParamCentered<RoundSmallBlackKnobLockable>(Vec(22.337, 34.295), module, Mix4::LEVEL_PARAMS+0));
    addParam(createLockableParamCentered<RoundSmallBlackKnobLockable>(Vec(22.337, 66.535), module, Mix4::LEVEL_PARAMS+1));
    addParam(createLockableParamCentered<RoundSmallBlackKnobLockable>(Vec(22.337, 98.775), module, Mix4::LEVEL_PARAMS+2));
    addParam(createLockableParamCentered<RoundSmallBlackKnobLockable>(Vec(22.337,131.014), module, Mix4::LEVEL_PARAMS+3));
    addParam(createLockableParamCentered<RoundBlackKnobLockable>(Vec(22.337,168.254), module, Mix4::MIX_LEVEL_PARAM));
    addParam(createLockableParamCentered<ModeSwitch>(Vec(37.491,50.415), module, Mix4::MODE_PARAM));
    addParam(createLockableParamCentered<DCBlockSwitch>(Vec(37.491,82.655), module, Mix4::DCBLOCK_PARAM));
    addParam(createLockableParamCentered<ClipSwitch>(Vec(37.491,114.895), module, Mix4::CLIP_PARAM));

    addInput(createInputCentered<PJ301MPort>(Vec(22.337,201.993), module, Mix4::INPUTS+0));
    addInput(createInputCentered<PJ301MPort>(Vec(22.337,235.233), module, Mix4::INPUTS+1));
    addInput(createInputCentered<PJ301MPort>(Vec(22.337,268.473), module, Mix4::INPUTS+2));
    addInput(createInputCentered<PJ301MPort>(Vec(22.337,301.712), module, Mix4::INPUTS+3));
    addOutput(createOutputCentered<PJ301MPort>(Vec(22.337,340.434), module, Mix4::MIX_OUTPUT));
  }

};

Model* modelMix4 = createModel<Mix4, Mix4Widget>("Mix4");
