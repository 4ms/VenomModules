#include "plugin.hpp"


struct Recurse : Module {
  enum ParamId {
    COUNT_PARAM,
    SCALE_PARAM,
    OFFSET_PARAM,
    TIMING_PARAM,
    ORDER_PARAM,
    PARAMS_LEN
  };
  enum InputId {
    SCALE_INPUT,
    OFFSET_INPUT,
    RETURN_INPUT,
    IN_INPUT,
    INPUTS_LEN
  };
  enum OutputId {
    SEND_OUTPUT,
    OUT_OUTPUT,
    OUTPUTS_LEN
  };
  enum LightId {
    SCALE_LIGHT,
    OFFSET_LIGHT,
    LIGHTS_LEN
  };

  int recurCount = 1;
  bool recurCountErr = false;
  int order = 0;
  int oldOrder = -1;

  enum ModTiming {
    PRE_START_1,
    PRE_START_N,
    POST_RETURN_N,
    POST_RETURN_1
  };

  Recurse() {
    struct TimingQuantity : ParamQuantity {
      std::string getDisplayValueString() override {
        Recurse* module = reinterpret_cast<Recurse*>(this->module);
        int val = static_cast<int>(module->params[Recurse::TIMING_PARAM].getValue());
        switch (val) {
          case 0: return "Before 1st send";
          case 1: return "Before all sends";
          case 2: return "After all returns";
          case 3: return "After last return";
          default: return "Error";
        };
      }
    };
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(COUNT_PARAM, 1.f, 16.f, 1.f, "Recursion Count", "");
    configParam(SCALE_PARAM, -10.f, 10.f, 1.f, "Scale", "");
    configParam(OFFSET_PARAM, -10.f, 10.f, 0.f, "Offset", " V");
    configParam<TimingQuantity>(TIMING_PARAM, 0.f, 3.f, 0.f, "Modulation Timing", "");
    configInput(SCALE_INPUT, "Scale");
    configInput(OFFSET_INPUT, "Offset");
    configInput(RETURN_INPUT, "Return");
    configInput(IN_INPUT, "Signal");
    configOutput(SEND_OUTPUT, "Send");
    configOutput(OUT_OUTPUT, "Signal");
    configBypass(IN_INPUT, OUT_OUTPUT);
  }

  void onReset() override {
    recurCount = 1;
    recurCountErr = false;
  }

  void process(const ProcessArgs& args) override {
    recurCount = static_cast<int>(params[COUNT_PARAM].getValue());
    int inChannels = std::max({1, inputs[IN_INPUT].getChannels()});
    recurCountErr = (inChannels > 16 / recurCount);
    int outChannels = recurCountErr ? 16 / recurCount : inChannels;
    ModTiming timing = static_cast<ModTiming>(params[TIMING_PARAM].getValue());
    float scaleParam = params[SCALE_PARAM].getValue();
    float offsetParam = params[OFFSET_PARAM].getValue();
    bool sendConnected = outputs[SEND_OUTPUT].isConnected();
    bool returnConnected = inputs[RETURN_INPUT].isConnected();
    bool mod = inputs[SCALE_INPUT].isConnected() || inputs[OFFSET_INPUT].isConnected() || scaleParam != 1.0f || offsetParam != 0.0f;
    float scale = 0.f;
    float offset = 0.f;
    for (int c=0; c<outChannels; c++) {
      float rtn = inputs[IN_INPUT].getVoltage(c);
      if (mod) {
        scale = inputs[SCALE_INPUT].getNormalVoltage(1.0f, c) * scaleParam;
        offset = inputs[OFFSET_INPUT].getNormalVoltage(0.0f, c) + offsetParam;
        if (timing == PRE_START_1)
          rtn = order==0 ? rtn * scale + offset : (rtn + offset) * scale;
      }
      for (int o=c*recurCount, end=o+recurCount; o<end; o++) {
         if (mod && timing == PRE_START_N)
           rtn = order==0 ? rtn * scale + offset : (rtn + offset) * scale;
         if (sendConnected)
           outputs[SEND_OUTPUT].setVoltage(rtn, o);
         if (returnConnected)
           rtn = inputs[RETURN_INPUT].getVoltage(o);
         if (mod && timing == POST_RETURN_N)
           rtn = order==0 ? rtn * scale + offset : (rtn + offset) * scale;
      }
      if (mod && timing == POST_RETURN_1)
        rtn = order==0 ? rtn * scale + offset : (rtn + offset) * scale;
      outputs[OUT_OUTPUT].setVoltage(rtn, c);
    }
    for (int c=outChannels; c<inChannels; c++)
      outputs[OUT_OUTPUT].setVoltage(0.0f, c);
    outputs[SEND_OUTPUT].setChannels(outChannels * recurCount);
    outputs[OUT_OUTPUT].setChannels(outChannels);

    if (order != oldOrder) {
      oldOrder = order;
      lights[SCALE_LIGHT].setBrightness(order==0);
      lights[OFFSET_LIGHT].setBrightness(order==1);
    }

  }


  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "orderOp", json_integer(order));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* val = json_object_get(rootJ, "orderOp");
    if (val)
      order = json_integer_value(val);
  }

};

struct CountDisplay : ChannelDisplay {
  Recurse* module;
  void step() override {
    if (module) {
      text = string::f("%d", module->recurCount);
      fgColor = module->recurCountErr ? SCHEME_RED : SCHEME_YELLOW;
    } else {
      text = "16";
      fgColor = SCHEME_YELLOW;
    }
  }
};


struct RecurseWidget : ModuleWidget {
  RecurseWidget(Recurse* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/Recurse.svg")));
    CountDisplay* countDisplay = createWidget<CountDisplay>(mm2px(Vec(3.5, 39.8)));
    countDisplay->box.size = mm2px(Vec(8.197, 8.197));
    countDisplay->module = module;
    addChild(countDisplay);
    addParam(createParamCentered<RoundSmallBlackKnobSnap>(mm2px(Vec(18.134, 43.87)), module, Recurse::COUNT_PARAM));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.299, 58.3)), module, Recurse::SCALE_INPUT));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(18.136, 58.3)), module, Recurse::SCALE_PARAM));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.297, 72.75)), module, Recurse::OFFSET_INPUT));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(18.134, 72.75)), module, Recurse::OFFSET_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnobSnap>(mm2px(Vec(12.7155, 84.50)), module, Recurse::TIMING_PARAM));
    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.297, 101.55)), module, Recurse::SEND_OUTPUT));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(18.134, 101.55)), module, Recurse::RETURN_INPUT));
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.297, 116.0)), module, Recurse::IN_INPUT));
    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.134, 116.0)), module, Recurse::OUT_OUTPUT));

    addChild(createLightCentered<TinyLight<YlwLight<>>>(mm2px(Vec(12.7115, 58.3)), module, Recurse::SCALE_LIGHT));
    addChild(createLightCentered<TinyLight<YlwLight<>>>(mm2px(Vec(12.7115, 72.75)), module, Recurse::OFFSET_LIGHT));
  }

  void appendContextMenu(Menu* menu) override {
    Recurse* module = dynamic_cast<Recurse*>(this->module);
    assert(module);
    menu->addChild(new MenuSeparator);
    std::vector<std::string> orderLabels;
    orderLabels.push_back("Scale before offset");
    orderLabels.push_back("Offset before scale");
    menu->addChild(createIndexSubmenuItem("Order of operation", orderLabels,
      [=]() {return module->order;},
      [=](int i) {module->order = i;}
    ));
  }

};

Model* modelRecurse = createModel<Recurse, RecurseWidget>("Recurse");
