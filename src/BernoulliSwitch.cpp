// Venom Modules (c) 2022 Dave Benham
// Licensed under GNU GPLv3

#include "plugin.hpp"
#include "ThemeStrings.hpp"
#include "OversampleFilter.hpp"

#define LIGHT_OFF 0.02f
#define FADE_RATE 400.f

#define MODULE_NAME BernoulliSwitch
static const std::string moduleName = "BernoulliSwitch";


struct BernoulliSwitch : Module {
  enum ParamId {
    PROB_PARAM,
    TRIG_PARAM,
    MODE_PARAM,
    RISE_PARAM,
    FALL_PARAM,
    OFFSET_A_PARAM,
    OFFSET_B_PARAM,
    SCALE_A_PARAM,
    SCALE_B_PARAM,
    PARAMS_LEN
  };
  enum InputId {
    A_INPUT,
    B_INPUT,
    TRIG_INPUT,
    PROB_INPUT,
    INPUTS_LEN
  };
  enum OutputId {
    A_OUTPUT,
    B_OUTPUT,
    OUTPUTS_LEN
  };
  enum LightId {
    NO_SWAP_LIGHT,
    SWAP_LIGHT,
    TRIG_LIGHT,
    ENUMS(AUDIO_LIGHT, 2),
    POLY_SENSE_ALL_LIGHT,
    LIGHTS_LEN
  };
  enum ProbMode {
    TOGGLE_MODE,
    SWAP_MODE,
    GATE_MODE
  };

  dsp::SchmittTrigger trig[PORT_MAX_CHANNELS];
  bool swap[PORT_MAX_CHANNELS];
  dsp::SlewLimiter fade[PORT_MAX_CHANNELS];
  int oldChannels = 0;
  int lightChannel = 0;
  bool lightOff = false;
  bool inputPolyControl = false;
  std::vector<int> oversampleValues = {1,1,2,4,8,16};
  int audioProc = 0;
  int oldAudioProc = -1;
  bool deClick = false;
  int oversample = 0;
  
  OversampleFilter_4 aUpSample[4], bUpSample[4],
                     aDownSample[4], bDownSample[4],
                     trigUpSample[4];

  #include "ThemeModVars.hpp"

  BernoulliSwitch() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(PROB_PARAM, 0.f, 1.f, 0.5f, "Probability", "%", 0.f, 100.f, 0.f);
    configButton(TRIG_PARAM, "Manual 10V Trigger");
    configSwitch(MODE_PARAM, 0, 2, 1, "Probability Mode", {"Toggle", "Swap", "Gate"});
    configParam(RISE_PARAM, -10.f, 10.f, 1.f, "Rise Threshold", " V");
    configParam(FALL_PARAM, -10.f, 10.f, 0.1f, "Fall Threshold", " V");
    configParam(OFFSET_A_PARAM, -10.f, 10.f, 0.f, "A Offset", " V");
    configParam(OFFSET_B_PARAM, -10.f, 10.f, 0.f, "B Offset", " V");
    configParam(SCALE_A_PARAM, -1.f, 1.f, 1.f, "A Scale", "");
    configParam(SCALE_B_PARAM, -1.f, 1.f, 1.f, "B Scale", "");
    configInput(A_INPUT, "A");
    configInput(B_INPUT, "B");
    configInput(TRIG_INPUT, "Trigger");
    configInput(PROB_INPUT, "Probability");
    configOutput(A_OUTPUT, "A");
    configOutput(B_OUTPUT, "B");
    configBypass(A_INPUT, A_OUTPUT);
    configBypass(B_INPUT, B_OUTPUT);
    lights[NO_SWAP_LIGHT].setBrightness(true);
    lights[SWAP_LIGHT].setBrightness(false);
    lights[POLY_SENSE_ALL_LIGHT].setBrightness(false);
    for (int i=0; i<PORT_MAX_CHANNELS; i++)
      fade[i].rise = fade[i].fall = FADE_RATE;
  }

  void onReset() override {
    oldChannels = 0;
    lights[NO_SWAP_LIGHT].setBrightness(true);
    lights[SWAP_LIGHT].setBrightness(false);
  }

  void process(const ProcessArgs& args) override {
    using float_4 = simd::float_4;
    float_4 aOut[4], bOut[4];
    float scaleA = params[SCALE_A_PARAM].getValue(),
          scaleB = params[SCALE_B_PARAM].getValue(),
          offA = params[OFFSET_A_PARAM].getValue(),
          offB = params[OFFSET_B_PARAM].getValue(),
          rise = params[RISE_PARAM].getValue(),
          fall = params[FALL_PARAM].getValue(),
          probOff = params[PROB_PARAM].getValue(),
          manual = params[TRIG_PARAM].getValue() > 0.f ? 10.f : 0.f;
    bool invTrig = rise < fall;
    int aChannels = std::max(1, inputs[A_INPUT].getChannels());
    int bChannels = std::max(1, inputs[B_INPUT].getChannels());
    int mode = static_cast<int>(params[MODE_PARAM].getValue());
    lights[TRIG_LIGHT].setBrightness(manual ? 1.f : LIGHT_OFF);
    if (invTrig) {
      rise = -rise;
      fall = -fall;
    }
    int channels = inputPolyControl ?
      std::max({ 1, aChannels, bChannels,
        inputs[TRIG_INPUT].getChannels(), inputs[PROB_INPUT].getChannels()
      }) :
      std::max({ 1,
        inputs[TRIG_INPUT].getChannels(), inputs[PROB_INPUT].getChannels()
      });
    int xChannels = channels;
    if (channels > oldChannels) {
      for (int c=oldChannels; c<channels; c++){
        trig[c].reset();
        swap[c] = false;
        fade[c].out = 0.f;
      }
      oldChannels = channels;
    }
    if (!lightOff && lightChannel >= channels) {
      lights[NO_SWAP_LIGHT].setBrightness(false);
      lights[SWAP_LIGHT].setBrightness(false);
      lightOff = true;
    }
    if (lightOff && lightChannel < channels) {
      lights[NO_SWAP_LIGHT].setBrightness(!swap[lightChannel]);
      lights[SWAP_LIGHT].setBrightness(swap[lightChannel]);
      lightOff = false;
    }
    if (audioProc != oldAudioProc) {
      oldAudioProc = audioProc;
      oversample = oversampleValues[audioProc];
      deClick = (audioProc == 1);
      lights[AUDIO_LIGHT].setBrightness(deClick);
      lights[AUDIO_LIGHT+1].setBrightness(audioProc>1);
      for (int c=0; c<4; c++) {
        aUpSample[c].setOversample(oversample);
        bUpSample[c].setOversample(oversample);
        aDownSample[c].setOversample(oversample);
        bDownSample[c].setOversample(oversample);
        trigUpSample[c].setOversample(oversample);
      }
    }
    lights[POLY_SENSE_ALL_LIGHT].setBrightness(inputPolyControl);

    float_4 trigIn0, trigIn;
    for (int c=0; c<channels; c+=4){
      float_4 prob = inputs[PROB_INPUT].getPolyVoltageSimd<float_4>(c)/10.f + probOff;
      trigIn = trigIn0 = inputs[TRIG_INPUT].getPolyVoltageSimd<float_4>(c) + manual;
      float_4 aIn, bIn, swapGain, remainderGain;
      for (int i=0; i<oversample; i++) {
        if (oversample > 1)
          trigIn = trigUpSample[c].process(i ? float_4::zero() : trigIn * oversample);
        for (int j=0; j<4 && c+j<channels; j++) {
          if(trig[c+j].process(invTrig ? -trigIn.s[j] : trigIn.s[j], fall, rise)){
            bool toss = (prob.s[c+j] == 1.0f || random::uniform() < prob.s[c+j]);
            switch(mode) {
              case TOGGLE_MODE:
                if (toss) swap[c+j] = !swap[c+j];
                break;
              case SWAP_MODE:
                swap[c+j] = toss;
                break;
              case GATE_MODE:
                swap[c+j] = !toss;
                break;
            }
            if (i == oversample-1 && c+j == lightChannel) {
              lights[NO_SWAP_LIGHT].setBrightness(!swap[c+j]);
              lights[SWAP_LIGHT].setBrightness(swap[c+j]);
            }
          }
          if (mode == GATE_MODE && !swap[c+j] && !trig[c+j].isHigh()) {
            swap[c] = true;
            if (i == oversample-1 && c+j == lightChannel){
              lights[NO_SWAP_LIGHT].setBrightness(false);
              lights[SWAP_LIGHT].setBrightness(true);
            }
          }
          if (deClick)
            fade[c+j].process(args.sampleTime, swap[c+j]);
          else
            fade[c+j].out = swap[c+j];
        }

        int c2End = c+1;
        if (channels == 1 && !inputPolyControl)
          c2End = xChannels = aChannels > bChannels ? aChannels : bChannels;
        for (int c2=c; c2<c2End; c2+=4) {
          int c0 = c2/4;
          aIn = i ? float_4::zero() : inputs[A_INPUT].getNormalPolyVoltageSimd<float_4>(trigIn0[c/4], c2) * scaleA + offA;
          bIn = i ? float_4::zero() : inputs[B_INPUT].getPolyVoltageSimd<float_4>(c2) * scaleB + offB;
          if (oversample>1) {
            aIn = aUpSample[c0].process(aIn * oversample);
            bIn = bUpSample[c0].process(bIn * oversample);
          }
          swapGain = (channels == 1 && !inputPolyControl ? float_4(fade[0].out) : float_4(fade[c].out, fade[c+1].out, fade[c+2].out, fade[c+3].out));
          remainderGain = 1.f - swapGain;
          aOut[c0] = aIn*remainderGain + bIn*swapGain;
          bOut[c0] = bIn*remainderGain + aIn*swapGain;
          if (oversample>1) {
            aOut[c0] = aDownSample[c0].process(aOut[c0]);
            bOut[c0] = bDownSample[c0].process(bOut[c0]);
          }
        }
      }
    }
    for (int c=0; c<xChannels; c+=4) {
      outputs[A_OUTPUT].setVoltageSimd(aOut[c/4], c);
      outputs[B_OUTPUT].setVoltageSimd(bOut[c/4], c);
    }
    outputs[A_OUTPUT].setChannels(xChannels);
    outputs[B_OUTPUT].setChannels(xChannels);
  }


  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "monitorChannel", json_integer(lightChannel));
    json_object_set_new(rootJ, "inputPolyControl", json_boolean(inputPolyControl));
    json_object_set_new(rootJ, "audioProc", json_integer(audioProc));
    #include "ThemeToJson.hpp"
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* val;
    if ((val = json_object_get(rootJ, "monitorChannel")))
      lightChannel = json_integer_value(val);
    if ((val = json_object_get(rootJ, "inputPolyControl")))
      inputPolyControl = json_boolean_value(val);
    if ((val = json_object_get(rootJ, "audioProc")))
      audioProc = json_integer_value(val);
    #include "ThemeFromJson.hpp"
  }

};

struct BernoulliSwitchWidget : ModuleWidget {
  BernoulliSwitchWidget(BernoulliSwitch* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, faceplatePath(moduleName, module ? module->currentThemeStr() : themes[getDefaultTheme()]))));

    addChild(createLightCentered<SmallSimpleLight<YellowLight>>(mm2px(Vec(5.0, 18.75)), module, BernoulliSwitch::NO_SWAP_LIGHT));
    addChild(createLightCentered<SmallSimpleLight<YellowLight>>(mm2px(Vec(20.431, 18.75)), module, BernoulliSwitch::SWAP_LIGHT));

    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(12.7155, 18.75)), module, BernoulliSwitch::PROB_PARAM));
    addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(6.5, 31.5)), module, BernoulliSwitch::TRIG_PARAM, BernoulliSwitch::TRIG_LIGHT));
    addParam(createParam<CKSSThree>(mm2px(Vec(17.5, 25.0)), module, BernoulliSwitch::MODE_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(7.297, 43.87)), module, BernoulliSwitch::RISE_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(18.134, 43.87)), module, BernoulliSwitch::FALL_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(7.297, 58.3)), module, BernoulliSwitch::OFFSET_A_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(18.136, 58.3)), module, BernoulliSwitch::OFFSET_B_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(7.297, 72.75)), module, BernoulliSwitch::SCALE_A_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(18.136, 72.75)), module, BernoulliSwitch::SCALE_B_PARAM));

    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.297, 87.10)), module, BernoulliSwitch::A_INPUT));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.134, 87.10)), module, BernoulliSwitch::B_INPUT));
    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.297, 101.55)), module, BernoulliSwitch::A_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 101.55)), module, BernoulliSwitch::B_OUTPUT));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.297, 116.0)), module, BernoulliSwitch::TRIG_INPUT));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.134, 116.0)), module, BernoulliSwitch::PROB_INPUT));

    addChild(createLightCentered<SmallSimpleLight<YellowLight>>(mm2px(Vec(12.7155, 83.9)), module, BernoulliSwitch::POLY_SENSE_ALL_LIGHT));
    addChild(createLightCentered<SmallSimpleLight<RedBlueLight<>>>(mm2px(Vec(12.7155, 98.35)), module, BernoulliSwitch::AUDIO_LIGHT));
  }

  void appendContextMenu(Menu* menu) override {
    BernoulliSwitch* module = dynamic_cast<BernoulliSwitch*>(this->module);
    assert(module);
    menu->addChild(new MenuSeparator);
    std::vector<std::string> lightChannelLabels;
    for (int i=1; i<=16; i++)
      lightChannelLabels.push_back(std::to_string(i));
    lightChannelLabels.push_back("Off");
    menu->addChild(createIndexPtrSubmenuItem(
      "Polyphony control",
      {"Trig and Prob only", "All inputs"},
      &module->inputPolyControl
    ));
    menu->addChild(createIndexSubmenuItem("Monitor channel", lightChannelLabels,
      [=]() {return module->lightChannel;},
      [=](int i) {
        module->lightChannel = i;
        module->lights[BernoulliSwitch::NO_SWAP_LIGHT].setBrightness(i > module->oldChannels ? false : !module->swap[i]);
        module->lights[BernoulliSwitch::SWAP_LIGHT].setBrightness(i > module->oldChannels ? false : module->swap[i]);
      }
    ));
    menu->addChild(createIndexPtrSubmenuItem(
      "Audio process",
      {"Off","Antipop crossfade","oversample x2","oversample x4","oversample x8","oversample x16"},
      &module->audioProc
    ));
    #include "ThemeMenu.hpp"
  }

  #include "ThemeStep.hpp"
};

Model* modelBernoulliSwitch = createModel<BernoulliSwitch, BernoulliSwitchWidget>("BernoulliSwitch");
