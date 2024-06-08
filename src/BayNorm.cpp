// Venom Modules (c) 2023, 2024 Dave Benham
// Licensed under GNU GPLv3

#include "BayModule.hpp"

struct BayNorm : BayOutputModule {

  BayNorm() {
    venomConfig(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    for (int i=0; i < OUTPUTS_LEN; i++) {
      configInput(POLY_INPUT+i, string::f("Port %d", i + 1));
      configOutput(POLY_OUTPUT+i, string::f("Port %d", i + 1));
    }
  }

  void process(const ProcessArgs& args) override {
    BayOutputModule::process(args);
    if (srcMod) {
      for (int i=0; i<OUTPUTS_LEN; i++) {
        int cnt = std::max(srcMod->inputs[i].getChannels(), inputs[i].getChannels());
        for (int c=0; c<cnt; c++)
          outputs[i].setVoltage(srcMod->inputs[i].getNormalVoltage(inputs[i].getVoltage(c), c), c);
        outputs[i].setChannels(cnt);
      }
    }
    else {
      for (int i=0; i<OUTPUTS_LEN; i++) {
        int cnt = inputs[i].getChannels();
        for (int c=0; c<cnt; c++)
          outputs[i].setVoltage(inputs[i].getVoltage(c), c);
        outputs[i].setChannels(cnt);
      }
    }
  }
  
};

struct BayNormWidget : BayOutputModuleWidget {

  BayNormWidget(BayNorm* module) {
    module->bayOutputType = 1;
    setModule(module);
    setVenomPanel("BayNorm");

    for (int i=0; i<BayNorm::OUTPUTS_LEN; i++) {
      addInput(createInputCentered<PolyPort>(Vec(21.5f,48.5f+i*42), module, BayNorm::POLY_INPUT + i));
      addOutput(createOutputCentered<PolyPort>(Vec(53.5f,48.5f+i*42), module, BayNorm::POLY_OUTPUT + i));
    }
  }

};

Model* modelBayNorm = createModel<BayNorm, BayNormWidget>("BayNorm");
