// Venom Modules (c) 2023, 2024 Dave Benham
// Licensed under GNU GPLv3

#include "plugin.hpp"
#include "Filter.hpp"
#include "math.hpp"
#include <float.h>

#define LIGHT_ON 1.f
#define LIGHT_OFF 0.02f

#define GRID_PARAM  SIN_SHAPE_PARAM
#define ASGN_PARAM  SIN_ASIGN_PARAM
#define GRID_INPUT  SIN_SHAPE_INPUT
#define GRID_LIGHT  SIN_SHAPE_LIGHT
#define GRID_OUTPUT SIN_OUTPUT
#define SIN 0
#define TRI 1
#define SQR 2
#define SAW 3
#define MIX 4

struct Oscillator : VenomModule {
 
  enum ParamId {
    MODE_PARAM,
    OVER_PARAM,
    PW_PARAM,
    MIXSHP_PARAM,
    DC_PARAM,
    FREQ_PARAM,
    OCTAVE_PARAM,
    UNUSED_PARAM,
    EXP_PARAM,
    LIN_PARAM,
    
    SIN_SHAPE_PARAM,
    TRI_SHAPE_PARAM,
    SQR_SHAPE_PARAM,
    SAW_SHAPE_PARAM,
    MIX_SHAPE_PARAM,
    SIN_SHAPE_AMT_PARAM,
    TRI_SHAPE_AMT_PARAM,
    SQR_SHAPE_AMT_PARAM,
    SAW_SHAPE_AMT_PARAM,
    MIX_SHAPE_AMT_PARAM,

    SIN_PHASE_PARAM,
    TRI_PHASE_PARAM,
    SQR_PHASE_PARAM,
    SAW_PHASE_PARAM,
    MIX_PHASE_PARAM,
    SIN_PHASE_AMT_PARAM,
    TRI_PHASE_AMT_PARAM,
    SQR_PHASE_AMT_PARAM,
    SAW_PHASE_AMT_PARAM,
    MIX_PHASE_AMT_PARAM,

    SIN_OFFSET_PARAM,
    TRI_OFFSET_PARAM,
    SQR_OFFSET_PARAM,
    SAW_OFFSET_PARAM,
    MIX_OFFSET_PARAM,
    SIN_OFFSET_AMT_PARAM,
    TRI_OFFSET_AMT_PARAM,
    SQR_OFFSET_AMT_PARAM,
    SAW_OFFSET_AMT_PARAM,
    MIX_OFFSET_AMT_PARAM,

    SIN_LEVEL_PARAM,
    TRI_LEVEL_PARAM,
    SQR_LEVEL_PARAM,
    SAW_LEVEL_PARAM,
    MIX_LEVEL_PARAM,
    SIN_LEVEL_AMT_PARAM,
    TRI_LEVEL_AMT_PARAM,
    SQR_LEVEL_AMT_PARAM,
    SAW_LEVEL_AMT_PARAM,
    MIX_LEVEL_AMT_PARAM,

    SIN_ASIGN_PARAM,
    TRI_ASIGN_PARAM,
    SQR_ASIGN_PARAM,
    SAW_ASIGN_PARAM,

    PARAMS_LEN
  };
  
  enum InputId {
    EXP_INPUT,
    LIN_INPUT,
    EXP_DEPTH_INPUT,
    LIN_DEPTH_INPUT,
    VOCT_INPUT,
    SYNC_INPUT,
    
    SIN_SHAPE_INPUT,
    TRI_SHAPE_INPUT,
    SQR_SHAPE_INPUT,
    SAW_SHAPE_INPUT,
    MIX_SHAPE_INPUT,
    
    SIN_PHASE_INPUT,
    TRI_PHASE_INPUT,
    SQR_PHASE_INPUT,
    SAW_PHASE_INPUT,
    MIX_PHASE_INPUT,
    
    SIN_OFFSET_INPUT,
    TRI_OFFSET_INPUT,
    SQR_OFFSET_INPUT,
    SAW_OFFSET_INPUT,
    MIX_OFFSET_INPUT,
    
    SIN_LEVEL_INPUT,
    TRI_LEVEL_INPUT,
    SQR_LEVEL_INPUT,
    SAW_LEVEL_INPUT,
    MIX_LEVEL_INPUT,

    REV_INPUT,

    INPUTS_LEN
  };
  enum OutputId {
    SIN_OUTPUT,
    TRI_OUTPUT,
    SQR_OUTPUT,
    SAW_OUTPUT,
    MIX_OUTPUT,
    OUTPUTS_LEN
  };
  enum LightId {
    ENUMS(REV_LIGHT,2),
    ENUMS(EXP_LIGHT,2),
    ENUMS(LIN_LIGHT,2),
    ENUMS(SYNC_LIGHT,2),
    
    ENUMS(SIN_SHAPE_LIGHT,2),
    ENUMS(TRI_SHAPE_LIGHT,2),
    ENUMS(SQR_SHAPE_LIGHT,2),
    ENUMS(SAW_SHAPE_LIGHT,2),
    ENUMS(MIX_SHAPE_LIGHT,2),

    ENUMS(SIN_PHASE_LIGHT,2),
    ENUMS(TRI_PHASE_LIGHT,2),
    ENUMS(SQR_PHASE_LIGHT,2),
    ENUMS(SAW_PHASE_LIGHT,2),
    ENUMS(MIX_PHASE_LIGHT,2),

    ENUMS(SIN_OFFSET_LIGHT,2),
    ENUMS(TRI_OFFSET_LIGHT,2),
    ENUMS(SQR_OFFSET_LIGHT,2),
    ENUMS(SAW_OFFSET_LIGHT,2),
    ENUMS(MIX_OFFSET_LIGHT,2),

    ENUMS(SIN_LEVEL_LIGHT,2),
    ENUMS(TRI_LEVEL_LIGHT,2),
    ENUMS(SQR_LEVEL_LIGHT,2),
    ENUMS(SAW_LEVEL_LIGHT,2),
    ENUMS(MIX_LEVEL_LIGHT,2),

    LIGHTS_LEN
  };

  bool disableOver[INPUTS_LEN]{};
  bool softSync = false;
  bool alternate = false;
  bool lfo = false;
  using float_4 = simd::float_4;
  int oversample = -1;
  std::vector<int> oversampleValues = {1,2,4,8,16,32};
  OversampleFilter_4 expUpSample[4], linUpSample[4], revUpSample[4], syncUpSample[4],
                     shapeUpSample[4][5], phaseUpSample[4][5], offsetUpSample[4][5], levelUpSample[4][5],
                     outDownSample[4][5];
  float_4 phasor[4]{}, phasorDir[4]{1.f, 1.f, 1.f, 1.f};
  DCBlockFilter_4 dcBlockFilter[4][5];
  dsp::SchmittTrigger syncTrig[16], revTrig[16];
  float modeFreq[3] = {dsp::FREQ_C4, 2.f, 100.f}, biasFreq = 0.02f;
  int currentMode = 0;
  int modeDefaultOver[3] = {2, 0, 2};
  static constexpr float maxFreq = 12000.f;
  
  struct PWQuantity : ParamQuantity {
    float getDisplayValue() override {
      float val = ParamQuantity::getDisplayValue();
      if (!(module->params[PW_PARAM].getValue()))
        val = clamp(val, 3.f, 97.f);
      return val;
    }
  };

  struct FreqQuantity : ParamQuantity {
    float getDisplayValue() override {
      Oscillator* mod = reinterpret_cast<Oscillator*>(this->module);
      int mode = static_cast<int>(mod->params[MODE_PARAM].getValue());
      float freq = 0.f;
      if (mode < 2)
        freq = pow(2.f, mod->params[FREQ_PARAM].getValue() + mod->params[OCTAVE_PARAM].getValue()) * mod->modeFreq[mode];
      else
        freq = mod->params[FREQ_PARAM].getValue() * mod->biasFreq;
      return freq < maxFreq ? freq : maxFreq;
    }
    void setDisplayValue(float v) override {
      Oscillator* mod = reinterpret_cast<Oscillator*>(this->module);
      int mode = static_cast<int>(mod->params[MODE_PARAM].getValue());
      if (v > maxFreq) v = maxFreq;
      if (mode < 2)
        setValue(clamp(std::log2f(v / mod->modeFreq[mode]) - mod->params[OCTAVE_PARAM].getValue(), -4.f, 4.f));
      else
        setValue(clamp(v / mod->biasFreq, -4.f, 4.f));
    }
  };


  Oscillator() {
    venomConfig(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

    configSwitch<FixedSwitchQuantity>(MODE_PARAM, 0.f, 2.f, 0.f, "Frequency Mode", {"Audio frequency", "Low frequency", "0Hz carrier"});
    configSwitch<FixedSwitchQuantity>(OVER_PARAM, 0.f, 5.f, 3.f, "Oversample", {"Off", "x2", "x4", "x8", "x16", "x32"});
    configSwitch<FixedSwitchQuantity>(PW_PARAM, 0.f, 1.f, 0.f, "Pulse Width Range", {"Limited 3%-97%", "Full 0%-100%"});
    configSwitch<FixedSwitchQuantity>(MIXSHP_PARAM, 0.f, 5.f, 0.f, "Mix Shape Mode", {"Sum (No shaping)", "Saturate Sum", "Fold Sum", "Average (No shaping)", "Saturate Average", "Fold Average"});
    configSwitch<FixedSwitchQuantity>(DC_PARAM,   0.f, 1.f, 0.f, "DC Block", {"Off", "On"});
    
    configParam<FreqQuantity>(FREQ_PARAM, -4.f, 4.f, 0.f, "Frequency", " Hz");
    configParam(OCTAVE_PARAM, -4.f, 4.f, 0.f, "Octave");
    configLight(REV_LIGHT, "Soft sync oversample indicator")->description = "off = none, yellow = oversampled, red = disabled";
    configInput(REV_INPUT, "Soft sync");
    configParam(EXP_PARAM, -1.f, 1.f, 0.f, "Exponential FM", "%", 0.f, 100.f);
    configParam(LIN_PARAM, -1.f, 1.f, 0.f, "Linear FM", "%", 0.f, 100.f);
    configInput(EXP_INPUT, "Exponential FM");
    configLight(EXP_LIGHT, "Exponential FM oversample indicator")->description = "off = none, yellow = oversampled, red = disabled";
    configInput(LIN_INPUT, "Linear FM");
    configLight(LIN_LIGHT, "Linear FM oversample indicator")->description = "off = none, yellow = oversampled, red = disabled";
    configInput(EXP_DEPTH_INPUT, "Exponential FM depth");
    configInput(LIN_DEPTH_INPUT, "Linear FM depth");
    configInput(VOCT_INPUT, "V/Oct");
    configInput(SYNC_INPUT, "Sync");
    configLight(SYNC_LIGHT, "Sync oversample indicator")->description = "off = none, yellow = oversampled, red = disabled";

    std::string xStr[5]{"Sine","Triangle","Square","Saw","Mix"};
    std::string yStr[4]{" shape"," phase"," offset"," level"};
    for (int y=0; y<4; y++){
      for (int x=0; x<5; x++){
        switch (y) {
          case 0: // shape
            switch (x) {
              case SQR:
                configParam<PWQuantity>(GRID_PARAM+y*10+x, -1.f, 1.f, 0.f, "Square pulse width", "%", 0.f, 50.f, 50.f);
                break;
              case MIX:
                configParam(GRID_PARAM+y*10+x, -1.f, 1.f, -1.f, xStr[x]+yStr[y], "%", 0.f, 50.f, 50.f);
                break;
              default:
                configParam(GRID_PARAM+y*10+x, -1.f, 1.f, 0.f, xStr[x]+yStr[y], "%", 0.f, 100.f);
            }
            break;
          case 1: // phase
            configParam(GRID_PARAM+y*10+x, -1.f, 1.f, 0.f, xStr[x]+yStr[y], "\u00B0", 0.f, 180.f);
            break;
          case 2: // offset
            configParam(GRID_PARAM+y*10+x, -1.f, 1.f, 0.f, xStr[x]+yStr[y], " V", 0.f, 5.f);
            break;
          case 3: // level
            configParam(GRID_PARAM+y*10+x, -1.f, 1.f, 0.f, xStr[x]+yStr[y], "%", 0.f, 100.f);
            break;
        }
        configParam(GRID_PARAM+y*10+x+5, -1.f, 1.f, 0.f, xStr[x]+yStr[y]+" CV amount", "%", 0.f, 100.f);
        configInput(GRID_INPUT+y*5+x, xStr[x]+yStr[y]+" CV");
        configLight(GRID_LIGHT+y*10+x*2, xStr[x]+yStr[y]+" oversample indicator")->description = "off = N/A, yellow = oversampled, red = disabled";
      }
    }
    for (int x=0; x<4; x++){
      configSwitch<FixedSwitchQuantity>(ASGN_PARAM+x, 0.f, 2.f, 0.f, xStr[x]+" level assignment", {"Mix output", xStr[x]+" output", "Both "+xStr[x]+" and Mix output"});
    }
    for (int x=0; x<5; x++){
      configOutput(GRID_OUTPUT+x, xStr[x]);
    }
    initDCBlock();
  }

  void onSampleRateChange(const SampleRateChangeEvent& e) override {
    initDCBlock();
  }
  
  void initDCBlock(){
    float sampleRate = settings::sampleRate;
    for (int i=0; i<4; i++){
      for (int j=0; j<5; j++){
        dcBlockFilter[i][j].init(sampleRate);
      }
    }
  }

  float_4 sinSimd_1000(float_4 t) {
    t = simd::ifelse(t > 500.f, 1000.f - t, t) * 0.002f - 0.5f;
    float_4 t2 = t * t;
    return -(((-0.540347 * t2 + 2.53566) * t2 - 5.16651) * t2 + 3.14159) * t;
  }

  void process(const ProcessArgs& args) override {
    VenomModule::process(args);

    int mode = static_cast<int>(params[MODE_PARAM].getValue());
    if (mode != currentMode) {
      currentMode = mode;
      params[OVER_PARAM].setValue(modeDefaultOver[mode]);
      paramQuantities[OVER_PARAM]->defaultValue = modeDefaultOver[mode];
      paramExtensions[OVER_PARAM].factoryDflt = modeDefaultOver[mode];
    }

    if (oversample != oversampleValues[params[OVER_PARAM].getValue()]) {
      oversample = oversampleValues[params[OVER_PARAM].getValue()];
      for (int i=0; i<4; i++){
        expUpSample[i].setOversample(oversample);
        linUpSample[i].setOversample(oversample);
        revUpSample[i].setOversample(oversample);
        syncUpSample[i].setOversample(oversample);
        for (int j=0; j<5; j++){
          shapeUpSample[i][j].setOversample(oversample);
          phaseUpSample[i][j].setOversample(oversample);
          offsetUpSample[i][j].setOversample(oversample);
          levelUpSample[i][j].setOversample(oversample);
          outDownSample[i][j].setOversample(oversample);
        }
      }
    }
    // get channel count
    int channels = 1;
    for (int i=0; i<INPUTS_LEN; i++) {
      int c = inputs[i].getChannels();
      if (c>channels)
        channels = c;
    }
    int simdCnt = (channels+3)/4;
    
    float_4 expIn{}, linIn{}, expDepthIn[4]{}, linDepthIn[4]{}, vOctIn[4]{}, revIn{}, syncIn{}, freq[4]{},
            shapeIn[5]{}, phaseIn[5]{}, offsetIn[5]{}, levelIn[5]{},
            sinOut[4]{}, triOut[4]{}, sqrOut[4]{}, sawOut[4]{}, mixOut[4]{},
            sinPhasor{}, triPhasor{}, sqrPhasor{}, sawPhasor{}, globalPhasor{};
    float vOctParm = mode<2 ? params[FREQ_PARAM].getValue() + params[OCTAVE_PARAM].getValue() : params[FREQ_PARAM].getValue();
    float k =  1000.f * args.sampleTime / oversample;
    
    if (alternate != (mode==2)) {
      alternate = !alternate;
      paramQuantities[FREQ_PARAM]->name = alternate ? "Bias" : "Frequency";
      paramQuantities[OCTAVE_PARAM]->name = alternate ? "Linear FM range" : "Octave";
      inputInfos[VOCT_INPUT]->name = alternate ? "Bias" : "V/Oct";
      paramQuantities[EXP_PARAM]->name = alternate ? "Unused" : "Exponential FM";
      inputInfos[EXP_INPUT]->name = alternate ? "Unused" : "Exponential FM";
      inputInfos[EXP_DEPTH_INPUT]->name = alternate ? "Unused" : "Exponential FM depth";

      paramExtensions[FREQ_PARAM].factoryName = paramQuantities[FREQ_PARAM]->name;
      paramExtensions[OCTAVE_PARAM].factoryName = paramQuantities[OCTAVE_PARAM]->name;
      inputExtensions[VOCT_INPUT].factoryName = inputInfos[VOCT_INPUT]->name;
      paramExtensions[EXP_PARAM].factoryName = paramQuantities[EXP_PARAM]->name;
      inputExtensions[EXP_INPUT].factoryName = inputInfos[EXP_INPUT]->name;
      inputExtensions[EXP_DEPTH_INPUT].factoryName = inputInfos[EXP_DEPTH_INPUT]->name;
    }
    
    if (softSync != inputs[REV_INPUT].isConnected()) {
      if (softSync) {
        for (int i=0; i<4; i++) phasorDir[i] = 1.f;
      }
      softSync = !softSync;
    }
    
    bool procSin = outputs[SIN_OUTPUT].isConnected() || (outputs[MIX_OUTPUT].isConnected() && params[SIN_ASIGN_PARAM].getValue() != 1.f);
    bool procTri = outputs[TRI_OUTPUT].isConnected() || (outputs[MIX_OUTPUT].isConnected() && params[TRI_ASIGN_PARAM].getValue() != 1.f);
    bool procSqr = outputs[SQR_OUTPUT].isConnected() || (outputs[MIX_OUTPUT].isConnected() && params[SQR_ASIGN_PARAM].getValue() != 1.f);
    bool procSaw = outputs[SAW_OUTPUT].isConnected() || (outputs[MIX_OUTPUT].isConnected() && params[SAW_ASIGN_PARAM].getValue() != 1.f);
    bool procMix = outputs[MIX_OUTPUT].isConnected();
    bool procOver[INPUTS_LEN]{};
    for (int i=0; i<INPUTS_LEN; i++)
      procOver[i] = oversample>1 && inputs[i].isConnected() && !disableOver[i];
    // main loops
    for (int o=0; o<oversample; o++){
      for (int s=0, c=0; s<simdCnt; s++, c+=4){
        float_4 level{}, mixDiv{};
        // Main Phasor
        if (!o) {
          if (!alternate) {
            if (s==0 || inputs[EXP_DEPTH_INPUT].isPolyphonic()) {
              expDepthIn[s] = simd::clamp( inputs[EXP_DEPTH_INPUT].getNormalPolyVoltageSimd<float_4>(5.f,c)/5.f, -1.f, 1.f);
            } else expDepthIn[s] = expDepthIn[0];
          }
          if (s==0 || inputs[LIN_DEPTH_INPUT].isPolyphonic()) {
            linDepthIn[s] = simd::clamp( inputs[LIN_DEPTH_INPUT].getNormalPolyVoltageSimd<float_4>(5.f,c)/5.f, -1.f, 1.f);
          } else linDepthIn[s] = linDepthIn[0];
          if (s==0 || inputs[VOCT_INPUT].isPolyphonic()) {
            vOctIn[s] = inputs[VOCT_INPUT].getPolyVoltageSimd<float_4>(c);
          } else vOctIn[s] = vOctIn[0];
        }
        if (!alternate) {
          if (s==0 || inputs[EXP_INPUT].isPolyphonic()) {
            expIn = (o && !disableOver[EXP_INPUT]) ? float_4::zero() : inputs[EXP_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[EXP_INPUT]){
              if (o==0) expIn *= oversample;
              expIn = expUpSample[s].process(expIn);
            }
          } // else preserve prior expIn value
        }
        if (s==0 || inputs[LIN_INPUT].isPolyphonic()) {
          linIn = (o && !disableOver[LIN_INPUT]) ? float_4::zero() : inputs[LIN_INPUT].getPolyVoltageSimd<float_4>(c);
          if (procOver[LIN_INPUT]){
            if (o==0) linIn *= oversample;
            linIn = linUpSample[s].process(linIn);
          }
        } // else prserve prior linIn value
        if (s==0 || inputs[MIX_PHASE_INPUT].isPolyphonic()) {
          phaseIn[MIX] = (o && !disableOver[MIX_PHASE_INPUT]) ? float_4::zero() : inputs[MIX_PHASE_INPUT].getPolyVoltageSimd<float_4>(c);
          if (procOver[MIX_PHASE_INPUT]){
            if (o==0) phaseIn[MIX] *= oversample;
            phaseIn[MIX] = phaseUpSample[s][MIX].process(phaseIn[MIX]);
          }
        } // else preserve prior phaseIn[MIX] value
        float_4 rev{};
        if (inputs[REV_INPUT].isConnected()) {
          if (s==0 || inputs[REV_INPUT].isPolyphonic()) {
            revIn = (o && !disableOver[REV_INPUT]) ? float_4::zero() : inputs[REV_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[REV_INPUT]){
              if (o==0) revIn *= oversample;
              revIn = revUpSample[s].process(revIn);
            }
          } // else preserve prior value
          for (int i=0; i<4; i++){
            rev[i] = revTrig[c+i].process(revIn[i], 0.2f, 2.f);
          }
        }
        float_4 sync{};
        if (inputs[SYNC_INPUT].isConnected()) {
          if (s==0 || inputs[SYNC_INPUT].isPolyphonic()) {
            syncIn = (o && !disableOver[SYNC_INPUT]) ? float_4::zero() : inputs[SYNC_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[SYNC_INPUT]){
              if (o==0) syncIn *= oversample;
              syncIn = syncUpSample[s].process(syncIn);
            }
          } // else preserve prior syncIn value
          for (int i=0; i<4; i++){
            sync[i] = syncTrig[c+i].process(syncIn[i], 0.2f, 2.f);
          }
        }
        if (!alternate) {
          freq[s] = vOctIn[s] + vOctParm + expIn*expDepthIn[s]*params[EXP_PARAM].getValue();
          freq[s] = dsp::exp2_taylor5(freq[s]) + linIn*linDepthIn[s]*params[LIN_PARAM].getValue();
        } else {
          freq[s] = (vOctParm + vOctIn[s])*biasFreq + linIn*linDepthIn[s]*params[LIN_PARAM].getValue()*((params[OCTAVE_PARAM].getValue()+4.f)*3.f+1.f);
        }
        freq[s] *= modeFreq[mode];
        freq[s] = simd::ifelse(freq[s] > maxFreq, maxFreq, freq[s]);
        phasorDir[s] = simd::ifelse(rev>0.f, phasorDir[s]*-1.f, phasorDir[s]);
        phasorDir[s] = simd::ifelse(sync>0.f, 1.f, phasorDir[s]);
        phasor[s] += freq[s] * phasorDir[s] * k;
        phasor[s] = simd::fmod(phasor[s], 1000.f);
        phasor[s] = simd::ifelse(phasor[s]<0.f, phasor[s]+1000.f, phasor[s]);
        phasor[s] = simd::ifelse(sync>0.f, float_4::zero(), phasor[s]);

        // Global (Mix) Phase
        globalPhasor = phasor[s] + (phaseIn[MIX]*params[MIX_PHASE_AMT_PARAM].getValue() + params[MIX_PHASE_PARAM].getValue()*2.f)*250.f;

        mixOut[s] = float_4::zero();

        // Sine
        if (procSin)
        {
          if (s==0 || inputs[SIN_SHAPE_INPUT].isPolyphonic()) {
            shapeIn[SIN] = (o && !disableOver[SIN_SHAPE_INPUT]) ? float_4::zero() : inputs[SIN_SHAPE_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[SIN_SHAPE_INPUT]){
              if (o==0) shapeIn[SIN] *= oversample;
              shapeIn[SIN] = shapeUpSample[s][SIN].process(shapeIn[SIN]);
            }
          } // preserve prior shapeIn[SIN] value
          float_4 shape = clamp(shapeIn[SIN]*params[SIN_SHAPE_AMT_PARAM].getValue()*0.1f + params[SIN_SHAPE_PARAM].getValue(), -1.f, 1.f);
          if (s==0 || inputs[SIN_PHASE_INPUT].isPolyphonic()) {
            phaseIn[SIN] = (o && !disableOver[SIN_PHASE_INPUT]) ? float_4::zero() : inputs[SIN_PHASE_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[SIN_PHASE_INPUT]){
              if (o==0) phaseIn[SIN] *= oversample;
              phaseIn[SIN] = phaseUpSample[s][SIN].process(phaseIn[SIN]);
            }
          } // else preserve prior phaseIn[SIN] value
          sinPhasor = globalPhasor + (phaseIn[SIN]*params[SIN_PHASE_AMT_PARAM].getValue() + params[SIN_PHASE_PARAM].getValue()*2.f)*250.f - 250.f;
          sinPhasor = simd::fmod(sinPhasor, 1000.f);
          sinPhasor = simd::ifelse(sinPhasor<0.f, sinPhasor+1000.f, sinPhasor);
          sinPhasor = sinSimd_1000(sinPhasor);
          sinOut[s] = crossfade(sinPhasor, ifelse(shape>0.f, 11.f*sinPhasor/(10.f*simd::abs(sinPhasor)+1.f), simd::sgn(sinPhasor)*simd::pow(sinPhasor,4)), ifelse(shape>0.f, shape, -shape))*5.f;

          if (s==0 || inputs[SIN_LEVEL_INPUT].isPolyphonic()) {
            levelIn[SIN] = (o && !disableOver[SIN_LEVEL_INPUT]) ? float_4::zero() : inputs[SIN_LEVEL_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[SIN_LEVEL_INPUT]){
              if (o==0) levelIn[SIN] *= oversample;
              levelIn[SIN] = levelUpSample[s][SIN].process(levelIn[SIN]);
            }
          } else levelIn[SIN] = levelIn[0][SIN];
          level = clamp(levelIn[SIN]*params[SIN_LEVEL_AMT_PARAM].getValue()*0.1f + params[SIN_LEVEL_PARAM].getValue(), -1.f, 1.f);
          if (params[SIN_ASIGN_PARAM].getValue()!=1) {
            mixOut[s] += sinOut[s] * level;
            mixDiv += simd::fabs(level);
          }

          if (s==0 || inputs[SIN_OFFSET_INPUT].isPolyphonic()) {
            offsetIn[SIN] = (o && !disableOver[SIN_OFFSET_INPUT]) ? float_4::zero() : inputs[SIN_OFFSET_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[SIN_OFFSET_INPUT]){
              if (o==0) offsetIn[SIN] *= oversample;
              offsetIn[SIN] = offsetUpSample[s][SIN].process(offsetIn[SIN]);
            }
          } // else preserve prior offsetIn[SIN] value
          sinOut[s] += clamp(offsetIn[SIN]*params[SIN_OFFSET_AMT_PARAM].getValue() + params[SIN_OFFSET_PARAM].getValue()*5.f, -5.f, 5.f);
          if (params[SIN_ASIGN_PARAM].getValue()!=0)
            sinOut[s] *= level;  
        }
        
        // Triangle
        if (procTri)
        {
          if (s==0 || inputs[TRI_SHAPE_INPUT].isPolyphonic()) {
            shapeIn[TRI] = (o && !disableOver[TRI_SHAPE_INPUT]) ? float_4::zero() : inputs[TRI_SHAPE_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[TRI_SHAPE_INPUT]){
              if (o==0) shapeIn[TRI] *= oversample;
              shapeIn[TRI] = shapeUpSample[s][TRI].process(shapeIn[TRI]);
            }
          } // else preserve prior shapeIn[TRI] value
          float_4 shape = clamp(shapeIn[TRI]*params[TRI_SHAPE_AMT_PARAM].getValue()*0.1f + params[TRI_SHAPE_PARAM].getValue(), -1.f, 1.f);
          if (s==0 || inputs[TRI_PHASE_INPUT].isPolyphonic()) {
            phaseIn[TRI] = (o && !disableOver[TRI_PHASE_INPUT]) ? float_4::zero() : inputs[TRI_PHASE_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[TRI_PHASE_INPUT]){
              if (o==0) phaseIn[TRI] *= oversample;
              phaseIn[TRI] = phaseUpSample[s][TRI].process(phaseIn[TRI]);
            }
          } // else preserve prior phaseIn[TRI] value
          triPhasor = globalPhasor + (phaseIn[TRI]*params[TRI_PHASE_AMT_PARAM].getValue() + params[TRI_PHASE_PARAM].getValue()*2.f)*250.f + 250.f;
          triPhasor = simd::fmod(triPhasor, 1000.f);
          triPhasor = simd::ifelse(triPhasor<0.f, triPhasor+1000.f, triPhasor);
          shape = simd::ifelse(triPhasor<500.f, shape, -shape);
          triPhasor = simd::ifelse(triPhasor<500.f, triPhasor*.002f, (1000.f-triPhasor)*.002f);
          triOut[s] = crossfade(triPhasor, ifelse(shape>0.f, 11.f*triPhasor/(10.f*simd::abs(triPhasor)+1.f), simd::sgn(triPhasor)*simd::pow(triPhasor,4)), ifelse(shape>0.f, shape, -shape))*10.f-5.f;

          if (s==0 || inputs[TRI_LEVEL_INPUT].isPolyphonic()) {
            levelIn[TRI] = (o && !disableOver[TRI_LEVEL_INPUT]) ? float_4::zero() : inputs[TRI_LEVEL_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[TRI_LEVEL_INPUT]){
              if (o==0) levelIn[TRI] *= oversample;
              levelIn[TRI] = levelUpSample[s][TRI].process(levelIn[TRI]);
            }
          } else levelIn[TRI] = levelIn[0][TRI];
          level = clamp(levelIn[TRI]*params[TRI_LEVEL_AMT_PARAM].getValue()*0.1f + params[TRI_LEVEL_PARAM].getValue(), -1.f, 1.f);
          if (params[TRI_ASIGN_PARAM].getValue()!=1){
            mixOut[s] += triOut[s] * level;
            mixDiv += simd::fabs(level);
          }

          if (s==0 || inputs[TRI_OFFSET_INPUT].isPolyphonic()) {
            offsetIn[TRI] = (o && !disableOver[TRI_OFFSET_INPUT]) ? float_4::zero() : inputs[TRI_OFFSET_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[TRI_OFFSET_INPUT]){
              if (o==0) offsetIn[TRI] *= oversample;
              offsetIn[TRI] = offsetUpSample[s][TRI].process(offsetIn[TRI]);
            }
          } // else preserve prior offsetIn[TRI] value
          triOut[s] += clamp(offsetIn[TRI]*params[TRI_OFFSET_AMT_PARAM].getValue() + params[TRI_OFFSET_PARAM].getValue()*5.f, -5.f, 5.f);
          if (params[TRI_ASIGN_PARAM].getValue()!=0)
            triOut[s] *= level;  
        }
        
        // Square
        if (procSqr)
        {
          if (s==0 || inputs[SQR_SHAPE_INPUT].isPolyphonic()) {
            shapeIn[SQR] = (o && !disableOver[SQR_SHAPE_INPUT]) ? float_4::zero() : inputs[SQR_SHAPE_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[SQR_SHAPE_INPUT]){
              if (o==0) shapeIn[SQR] *= oversample;
              shapeIn[SQR] = shapeUpSample[s][SQR].process(shapeIn[SQR]);
            }
          } // else preserve prior shapeIn[SQR] value
          float_4 flip = (shapeIn[SQR]*params[SQR_SHAPE_AMT_PARAM].getValue()*0.2f + params[SQR_SHAPE_PARAM].getValue() + 1.f) * 500.f;
          if (!params[PW_PARAM].getValue()) flip = clamp( flip, 30.f, 970.f );
          if (s==0 || inputs[SQR_PHASE_INPUT].isPolyphonic()) {
            phaseIn[SQR] = (o && !disableOver[SQR_PHASE_INPUT]) ? float_4::zero() : inputs[SQR_PHASE_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[SQR_PHASE_INPUT]){
              if (o==0) phaseIn[SQR] *= oversample;
              phaseIn[SQR] = phaseUpSample[s][SQR].process(phaseIn[SQR]);
            }
          } // else preserve prior phaseIn[SQR] value
          sqrPhasor = globalPhasor + (phaseIn[SQR]*params[SQR_PHASE_AMT_PARAM].getValue() + params[SQR_PHASE_PARAM].getValue()*2.f)*250.f;
          sqrPhasor = simd::fmod(sqrPhasor, 1000.f);
          sqrPhasor = simd::ifelse(sqrPhasor<0.f, sqrPhasor+1000.f, sqrPhasor);
          sqrOut[s] = simd::ifelse(sqrPhasor<flip, 5.f, -5.f);

          if (s==0 || inputs[SQR_LEVEL_INPUT].isPolyphonic()) {
            levelIn[SQR] = (o && !disableOver[SQR_LEVEL_INPUT]) ? float_4::zero() : inputs[SQR_LEVEL_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[SQR_LEVEL_INPUT]){
              if (o==0) levelIn[SQR] *= oversample;
              levelIn[SQR] = levelUpSample[s][SQR].process(levelIn[SQR]);
            }
          } else levelIn[SQR] = levelIn[0][SQR];
          level = clamp(levelIn[SQR]*params[SQR_LEVEL_AMT_PARAM].getValue()*0.1f + params[SQR_LEVEL_PARAM].getValue(), -1.f, 1.f);
          if (params[SQR_ASIGN_PARAM].getValue()!=1){
            mixOut[s] += sqrOut[s] * level;
            mixDiv += simd::fabs(level);
          }

          if (s==0 || inputs[SQR_OFFSET_INPUT].isPolyphonic()) {
            offsetIn[SQR] = (o && !disableOver[SQR_OFFSET_INPUT]) ? float_4::zero() : inputs[SQR_OFFSET_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[SQR_OFFSET_INPUT]){
              if (o==0) offsetIn[SQR] *= oversample;
              offsetIn[SQR] = offsetUpSample[s][SQR].process(offsetIn[SQR]);
            }
          } // else preserve prior offsetIn[SQR] value
          sqrOut[s] += clamp(offsetIn[SQR]*params[SQR_OFFSET_AMT_PARAM].getValue() + params[SQR_OFFSET_PARAM].getValue()*5.f, -5.f, 5.f);
          if (params[SQR_ASIGN_PARAM].getValue()!=0)
            sqrOut[s] *= level;  
        }
        
        // Saw
        if (procSaw)
        {
          if (s==0 || inputs[SAW_SHAPE_INPUT].isPolyphonic()) {
            shapeIn[SAW] = (o && !disableOver[SAW_SHAPE_INPUT]) ? float_4::zero() : inputs[SAW_SHAPE_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[SAW_SHAPE_INPUT]){
              if (o==0) shapeIn[SAW] *= oversample;
              shapeIn[SAW] = shapeUpSample[s][SAW].process(shapeIn[SAW]);
            }
          } // else preserve prior shapeIn[SAW] value
          float_4 shape = clamp(shapeIn[SAW]*params[SAW_SHAPE_AMT_PARAM].getValue()*0.1f + params[SAW_SHAPE_PARAM].getValue(), -1.f, 1.f);
          if (s==0 || inputs[SAW_PHASE_INPUT].isPolyphonic()) {
            phaseIn[SAW] = (o && !disableOver[SAW_PHASE_INPUT]) ? float_4::zero() : inputs[SAW_PHASE_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[SAW_PHASE_INPUT]){
              if (o==0) phaseIn[SAW] *= oversample;
              phaseIn[SAW] = phaseUpSample[s][SAW].process(phaseIn[SAW]);
            }
          } // else preserve prior phaseIn[SAW] value
          sawPhasor = globalPhasor + (phaseIn[SAW]*params[SAW_PHASE_AMT_PARAM].getValue() + params[SAW_PHASE_PARAM].getValue()*2.f)*250.f;
          sawPhasor = simd::fmod(sawPhasor, 1000.f);
          sawPhasor = simd::ifelse(sawPhasor<0.f, sawPhasor+1000.f, sawPhasor);
          sawPhasor *= 0.001f;
          sawOut[s] = crossfade(sawPhasor, ifelse(shape>0.f, 11.f*sawPhasor/(10.f*simd::abs(sawPhasor)+1.f), simd::sgn(sawPhasor)*simd::pow(sawPhasor,4)), ifelse(shape>0.f, shape, -shape))*10.f-5.f;

          if (s==0 || inputs[SAW_LEVEL_INPUT].isPolyphonic()) {
            levelIn[SAW] = (o && !disableOver[SAW_LEVEL_INPUT]) ? float_4::zero() : inputs[SAW_LEVEL_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[SAW_LEVEL_INPUT]){
              if (o==0) levelIn[SAW] *= oversample;
              levelIn[SAW] = levelUpSample[s][SAW].process(levelIn[SAW]);
            }
          } else levelIn[SAW] = levelIn[0][SAW];
          level = clamp(levelIn[SAW]*params[SAW_LEVEL_AMT_PARAM].getValue()*0.1f + params[SAW_LEVEL_PARAM].getValue(), -1.f, 1.f);
          if (params[SAW_ASIGN_PARAM].getValue()!=1){
            mixOut[s] += sawOut[s] * level;
            mixDiv += simd::fabs(level);
          }

          if (s==0 || inputs[SAW_OFFSET_INPUT].isPolyphonic()) {
            offsetIn[SAW] = (o && !disableOver[SAW_OFFSET_INPUT]) ? float_4::zero() : inputs[SAW_OFFSET_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[SAW_OFFSET_INPUT]){
              if (o==0) offsetIn[SAW] *= oversample;
              offsetIn[SAW] = offsetUpSample[s][SAW].process(offsetIn[SAW]);
            }
          } // else preserve prior offsetIn[SAW] value
          sawOut[s] += clamp(levelIn[SAW]*params[SAW_OFFSET_AMT_PARAM].getValue() + params[SAW_OFFSET_PARAM].getValue()*5.f, -5.f, 5.f);
          if (params[SAW_ASIGN_PARAM].getValue()!=0)
            sawOut[s] *= level;  
        }
        
        // Mix
        if (procMix) {
          int folds=10;
          if (params[MIXSHP_PARAM].getValue() > 2.5) {
            mixOut[s] = simd::ifelse(mixDiv>0.f, mixOut[s]/mixDiv, mixOut[s]);
            folds=3;
          }
          int typ = static_cast<int>(params[MIXSHP_PARAM].getValue()) % 3;
          if (typ) {
            if (s==0 || inputs[MIX_SHAPE_INPUT].isPolyphonic()) {
              shapeIn[MIX] = (o && !disableOver[MIX_SHAPE_INPUT]) ? float_4::zero() : inputs[MIX_SHAPE_INPUT].getPolyVoltageSimd<float_4>(c);
              if (procOver[MIX_SHAPE_INPUT]){
                if (o==0) shapeIn[MIX] *= oversample;
                shapeIn[MIX] = shapeUpSample[s][MIX].process(shapeIn[MIX]);
              }
            } // else preserve prior shapeIn[MIX] value
            float_4 drive = clamp(shapeIn[MIX]*params[MIX_SHAPE_AMT_PARAM].getValue() + params[MIX_SHAPE_PARAM].getValue()+1.f, 0.f, 3.f)*2.f + 1.f;
            if (typ==1){
              mixOut[s] = softClip<float_4>(mixOut[s]*2.f*drive)/2.f;
            }
            if (typ==2){
              mixOut[s] *= drive;
              float_4 clmp;
              for (int i=0; i<folds; i++){
                clmp = clamp(mixOut[s],-5,5);
                mixOut[s] = clmp + clmp - mixOut[s];
              }
            }
          }
          if (s==0 || inputs[MIX_OFFSET_INPUT].isPolyphonic()) {
            offsetIn[MIX] = (o && !disableOver[MIX_OFFSET_INPUT]) ? float_4::zero() : inputs[MIX_OFFSET_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[MIX_OFFSET_INPUT]){
              if (o==0) offsetIn[MIX] *= oversample;
              offsetIn[MIX] = offsetUpSample[s][MIX].process(offsetIn[MIX]);
            }
          } // else preserve prior offsetIn[MIX] value
          mixOut[s] += clamp(offsetIn[MIX]*params[MIX_OFFSET_AMT_PARAM].getValue() + params[MIX_OFFSET_PARAM].getValue()*5.f, -5.f, 5.f);
          if (s==0 || inputs[MIX_LEVEL_INPUT].isPolyphonic()) {
            levelIn[MIX] = (o && !disableOver[MIX_LEVEL_INPUT]) ? float_4::zero() : inputs[MIX_LEVEL_INPUT].getPolyVoltageSimd<float_4>(c);
            if (procOver[MIX_LEVEL_INPUT]){
              if (o==0) levelIn[MIX] *= oversample;
              levelIn[MIX] = levelUpSample[s][MIX].process(levelIn[MIX]);
            }
          } // else preserve prior levelIn[MIX] value
          mixOut[s] *= clamp(levelIn[MIX]*params[MIX_LEVEL_AMT_PARAM].getValue()*0.1f + params[MIX_LEVEL_PARAM].getValue(), -1.f, 1.f);
        }

        // FINAL PROCESSING
        // Remove DC offset
        if (params[DC_PARAM].getValue()) {
          if (outputs[SIN_OUTPUT].isConnected())
            sinOut[s] = dcBlockFilter[s][SIN].process(sinOut[s]);
          if (outputs[TRI_OUTPUT].isConnected())
            triOut[s] = dcBlockFilter[s][TRI].process(triOut[s]);
          if (outputs[SQR_OUTPUT].isConnected())
            sqrOut[s] = dcBlockFilter[s][SQR].process(sqrOut[s]);
          if (outputs[SAW_OUTPUT].isConnected())
            sawOut[s] = dcBlockFilter[s][SAW].process(sawOut[s]);
          if (outputs[MIX_OUTPUT].isConnected())
            mixOut[s] = dcBlockFilter[s][MIX].process(mixOut[s]);
        }
        // Downsample outputs
        if (oversample>1) {
          if (outputs[SIN_OUTPUT].isConnected())
            sinOut[s] = outDownSample[s][SIN].process(sinOut[s]);
          if (outputs[TRI_OUTPUT].isConnected())
            triOut[s] = outDownSample[s][TRI].process(triOut[s]);
          if (outputs[SQR_OUTPUT].isConnected())
            sqrOut[s] = outDownSample[s][SQR].process(sqrOut[s]);
          if (outputs[SAW_OUTPUT].isConnected())
            sawOut[s] = outDownSample[s][SAW].process(sawOut[s]);
          if (outputs[MIX_OUTPUT].isConnected())
            mixOut[s] = outDownSample[s][MIX].process(mixOut[s]);
        }
      }
    }
    
    float_4 out{};
    for (int s=0, c=0; s<simdCnt; s++, c+=4) {
      outputs[SIN_OUTPUT].setVoltageSimd( sinOut[s], c );
      outputs[TRI_OUTPUT].setVoltageSimd( triOut[s], c );
      outputs[SQR_OUTPUT].setVoltageSimd( sqrOut[s], c );
      outputs[SAW_OUTPUT].setVoltageSimd( sawOut[s], c );
      outputs[MIX_OUTPUT].setVoltageSimd( mixOut[s], c );
    }
    outputs[SIN_OUTPUT].setChannels(channels);
    outputs[TRI_OUTPUT].setChannels(channels);
    outputs[SQR_OUTPUT].setChannels(channels);
    outputs[SAW_OUTPUT].setChannels(channels);
    outputs[MIX_OUTPUT].setChannels(channels);
  }
  
  json_t* dataToJson() override {
    json_t* rootJ = VenomModule::dataToJson();
    json_t* array = json_array();
    for (int i=0; i<INPUTS_LEN; i++)
      json_array_append_new(array, json_boolean(disableOver[i]));
    json_object_set_new(rootJ, "disableOver", array);
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    VenomModule::dataFromJson(rootJ);
    json_t* array;
    json_t* val;
    size_t index;
    if ((array = json_object_get(rootJ, "disableOver"))) {
      json_array_foreach(array, index, val){
        disableOver[index] = json_boolean_value(val);
      }
    }
    currentMode = static_cast<int>(params[MODE_PARAM].getValue());
  }
  
};

struct OscillatorWidget : VenomWidget {
  
  struct ModeSwitch : GlowingSvgSwitchLockable {
    ModeSwitch() {
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallGreenButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallBlueButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallPurpleButtonSwitch.svg")));
    }
  };

  struct OverSwitch : GlowingSvgSwitchLockable {
    OverSwitch() {
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallOffButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallYellowButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallGreenButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallLightBlueButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallBlueButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallPurpleButtonSwitch.svg")));
    }
  };

  struct PWSwitch : GlowingSvgSwitchLockable {
    PWSwitch() {
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallOffButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallYellowButtonSwitch.svg")));
    }
  };

  struct MixShpSwitch : GlowingSvgSwitchLockable {
    MixShpSwitch() {
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallYellowButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallOrangeButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallPurpleButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallLightBlueButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallBlueButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallGreenButtonSwitch.svg")));
    }
  };

  struct DCBlockSwitch : GlowingSvgSwitchLockable {
    DCBlockSwitch() {
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallOffButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallYellowButtonSwitch.svg")));
    }
  };

  struct AssignSwitch : GlowingSvgSwitchLockable {
    AssignSwitch() {
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallYellowButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallBlueButtonSwitch.svg")));
      addFrame(Svg::load(asset::plugin(pluginInstance,"res/smallGreenButtonSwitch.svg")));
    }
  };
  
  struct OverPort : PolyPort {
    int portId;
    void appendContextMenu(Menu* menu) override {
      Oscillator* module = static_cast<Oscillator*>(this->module);
      menu->addChild(new MenuSeparator);
      menu->addChild(createBoolPtrMenuItem("Disable oversampling", "", &module->disableOver[portId]));
      PolyPort::appendContextMenu(menu);
    }
  };
  
  template <class TOverPort>
  TOverPort* createOverInputCentered(math::Vec pos, engine::Module* module, int inputId) {
    TOverPort* o = createInputCentered<TOverPort>(pos, module, inputId);
    o->portId = inputId;
    return o;
  }

  OscillatorWidget(Oscillator* module) {
    setModule(module);
    setVenomPanel("Oscillator");
    
    addParam(createLockableParamCentered<ModeSwitch>(Vec(14.5f,37.5f), module, Oscillator::MODE_PARAM));
    addParam(createLockableParamCentered<OverSwitch>(Vec(32.5f,37.5f), module, Oscillator::OVER_PARAM));
    addParam(createLockableParamCentered<PWSwitch>(Vec(50.5f,37.5f), module, Oscillator::PW_PARAM));
    addParam(createLockableParamCentered<MixShpSwitch>(Vec(68.5f,37.5f), module, Oscillator::MIXSHP_PARAM));
    addParam(createLockableParamCentered<DCBlockSwitch>(Vec(86.5f,37.5f), module, Oscillator::DC_PARAM));
    
    addParam(createLockableParamCentered<RoundHugeBlackKnobLockable>(Vec(46.5f,93.5f), module, Oscillator::FREQ_PARAM));
    addParam(createLockableParamCentered<RotarySwitch<RoundBlackKnobLockable>>(Vec(29.f,157.f), module, Oscillator::OCTAVE_PARAM));

    addInput(createInputCentered<OverPort>(Vec(64.f, 158.f), module, Oscillator::REV_INPUT));
    addChild(createLightCentered<SmallLight<YellowRedLight<>>>(Vec(77.5f, 146.5f), module, Oscillator::REV_LIGHT));

    addParam(createLockableParamCentered<RoundSmallBlackKnobLockable>(Vec(29.f,206.f), module, Oscillator::EXP_PARAM));
    addParam(createLockableParamCentered<RoundSmallBlackKnobLockable>(Vec(64.f,206.f), module, Oscillator::LIN_PARAM));
    addInput(createOverInputCentered<OverPort>(Vec(29.f, 241.5f), module, Oscillator::EXP_INPUT));
    addChild(createLightCentered<SmallLight<YellowRedLight<>>>(Vec(42.5f, 230.f), module, Oscillator::EXP_LIGHT));
    addInput(createOverInputCentered<OverPort>(Vec(64.f, 241.5f), module, Oscillator::LIN_INPUT));
    addChild(createLightCentered<SmallLight<YellowRedLight<>>>(Vec(77.5f, 230.f), module, Oscillator::LIN_LIGHT));
    addInput(createInputCentered<PolyPort>(Vec(29.f, 290.5f), module, Oscillator::EXP_DEPTH_INPUT));
    addInput(createInputCentered<PolyPort>(Vec(64.f, 290.5f), module, Oscillator::LIN_DEPTH_INPUT));
    addInput(createInputCentered<PolyPort>(Vec(29.f, 335.5f), module, Oscillator::VOCT_INPUT));
    addInput(createOverInputCentered<OverPort>(Vec(64.f, 335.5f), module, Oscillator::SYNC_INPUT));
    addChild(createLightCentered<SmallLight<YellowRedLight<>>>(Vec(77.5f, 324.f), module, Oscillator::SYNC_LIGHT));
    
    float dx = 45.f;
    float dy = 61.f;
    for (int y=0; y<4; y++) {
      for (int x=0; x<5; x++) {
        addParam(createLockableParamCentered<RoundTinyBlackKnobLockable>(Vec(119.5f+dx*x,59.5f+dy*y), module, Oscillator::GRID_PARAM+y*10+x));
        addParam(createLockableParamCentered<RoundTinyBlackKnobLockable>(Vec(140.5f+dx*x,59.5f+dy*y), module, Oscillator::GRID_PARAM+y*10+x+5));
        addInput(createOverInputCentered<OverPort>(Vec(130.f+dx*x,85.5f+dy*y), module, Oscillator::GRID_INPUT+y*5+x));
        addChild(createLightCentered<SmallLight<YellowRedLight<>>>(Vec(143.5f+dx*x, 74.f+dy*y), module, Oscillator::GRID_LIGHT+y*10+x*2));
      }
    }
    for (int x=0; x<4; x++) {
      addParam(createLockableParamCentered<AssignSwitch>(Vec(130.f+dx*x,291.5f), module, Oscillator::ASGN_PARAM+x));
    }
    for (int x=0; x<5; x++) {
      addOutput(createOutputCentered<PolyPort>(Vec(130.f+dx*x,335.5f), module, Oscillator::GRID_OUTPUT+x));
    }
  }

  void step() override {
    VenomWidget::step();
    Oscillator* mod = dynamic_cast<Oscillator*>(this->module);
    if(mod) {
      bool over = mod->params[Oscillator::OVER_PARAM].getValue();
      mod->lights[Oscillator::REV_LIGHT].setBrightness(over && !(mod->disableOver[Oscillator::REV_INPUT]) && mod->inputs[Oscillator::REV_INPUT].isConnected());
      mod->lights[Oscillator::REV_LIGHT+1].setBrightness(over && mod->disableOver[Oscillator::REV_INPUT] && mod->inputs[Oscillator::REV_INPUT].isConnected());
      mod->lights[Oscillator::EXP_LIGHT].setBrightness(over && !(mod->disableOver[Oscillator::EXP_INPUT]) && mod->inputs[Oscillator::EXP_INPUT].isConnected() && !(mod->alternate));
      mod->lights[Oscillator::EXP_LIGHT+1].setBrightness(over && mod->disableOver[Oscillator::EXP_INPUT] && mod->inputs[Oscillator::EXP_INPUT].isConnected() && !(mod->alternate));
      mod->lights[Oscillator::LIN_LIGHT].setBrightness(over && !(mod->disableOver[Oscillator::LIN_INPUT]) && mod->inputs[Oscillator::LIN_INPUT].isConnected());
      mod->lights[Oscillator::LIN_LIGHT+1].setBrightness(over && mod->disableOver[Oscillator::LIN_INPUT] && mod->inputs[Oscillator::LIN_INPUT].isConnected());
      mod->lights[Oscillator::SYNC_LIGHT].setBrightness(over && !(mod->disableOver[Oscillator::SYNC_INPUT]) && mod->inputs[Oscillator::SYNC_INPUT].isConnected());
      mod->lights[Oscillator::SYNC_LIGHT+1].setBrightness(over && mod->disableOver[Oscillator::SYNC_INPUT] && mod->inputs[Oscillator::SYNC_INPUT].isConnected());
      for (int y=0; y<4; y++) {
        for (int x=0; x<5; x++) {
          mod->lights[Oscillator::GRID_LIGHT+y*10+x*2].setBrightness(over && !(mod->disableOver[Oscillator::GRID_INPUT+y*5+x]) && mod->inputs[Oscillator::GRID_INPUT+y*5+x].isConnected());
          mod->lights[Oscillator::GRID_LIGHT+y*10+x*2+1].setBrightness(over && mod->disableOver[Oscillator::GRID_INPUT+y*5+x] && mod->inputs[Oscillator::GRID_INPUT+y*5+x].isConnected());
        }
      }
    }
  }

};

Model* modelOscillator = createModel<Oscillator, OscillatorWidget>("Oscillator");
