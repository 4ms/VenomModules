// Venom Modules (c) 2023, 2024 Dave Benham
// Licensed under GNU GPLv3

#include "plugin.hpp"

namespace Venom
{
void readDefaultThemes();
}

Plugin *pluginInstance;

void init(Plugin *p) {
	pluginInstance = p;

	// Add modules here
	p->addModel(modelVenomAD_ASR);
	p->addModel(modelVenomAuxClone);
#ifndef METAMODULE
	// Requires accessing other modules
	p->addModel(modelVenomBayInput);
	p->addModel(modelVenomBayNorm);
	p->addModel(modelVenomBayOutput);
#endif
	p->addModel(modelVenomBenjolinOsc);
	p->addModel(modelVenomBenjolinGatesExpander);
	p->addModel(modelVenomBenjolinVoltsExpander);
	p->addModel(modelVenomBernoulliSwitch);
	p->addModel(modelVenomBernoulliSwitchExpander);
#ifndef METAMODULE
	// Requires Rack expander chains
	p->addModel(modelVenomBlocker);
	// Requres accessing other modules
	p->addModel(modelVenomBypass);
#endif
	p->addModel(modelVenomCloneMerge);
	p->addModel(modelVenomCompare2);
	p->addModel(modelVenomCrossFade3D);
	p->addModel(modelVenomHQ);
	p->addModel(modelVenomKnob5);
	p->addModel(modelVenomLinearBeats);
	p->addModel(modelVenomLinearBeatsExpander);
	p->addModel(modelVenomLogic);
	p->addModel(modelVenomMerge4x2);
	p->addModel(modelVenomMergeSplit);
	p->addModel(modelVenomMix4);
	p->addModel(modelVenomMix4Stereo);
	p->addModel(modelVenomMixFade);
	p->addModel(modelVenomMixFade2);
	p->addModel(modelVenomMixMute);
	p->addModel(modelVenomMixOffset);
	p->addModel(modelVenomMixPan);
	p->addModel(modelVenomMixSend);
	p->addModel(modelVenomMixSolo);
#ifndef METAMODULE
	// GUI mouse required
	p->addModel(modelVenomMousePad);
#endif
	p->addModel(modelVenomMultiMerge);
	p->addModel(modelVenomMultiSplit);
	p->addModel(modelVenomSVF);
	p->addModel(modelVenomOscillator);
	p->addModel(modelVenomNORS_IQ);
#ifndef METAMODULE
	// TODO: test
	p->addModel(modelVenomNORSIQChord2Scale);
#endif
	p->addModel(modelVenomNullCable);
	p->addModel(modelVenomOctaver);
	p->addModel(modelVenomPan3D);
	p->addModel(modelVenomPolyClone);
	p->addModel(modelVenomPolyFade);
	p->addModel(modelVenomPolyMute);
	p->addModel(modelVenomPolyOffset);
	p->addModel(modelVenomPolyPrune);
	p->addModel(modelVenomPolySHASR);
	p->addModel(modelVenomPolyScale);
	p->addModel(modelVenomPolyUnison);
	p->addModel(modelVenomPush5);
	p->addModel(modelVenomQuadVCPolarizer);
#ifndef METAMODULE
	// TODO: test with 4-voice poly
	p->addModel(modelVenomRecurse);
	p->addModel(modelVenomRecurseStereo);
#endif
	p->addModel(modelVenomReformation);
	p->addModel(modelVenomRhythmExplorer);
	p->addModel(modelVenomREXCV);
	p->addModel(modelVenomShapedVCA);
	p->addModel(modelVenomSlew);
	p->addModel(modelVenomSphereToXYZ);
	p->addModel(modelVenomSplit4x2);
	p->addModel(modelVenomThru);
	p->addModel(modelVenomVCAMix4);
	p->addModel(modelVenomVCAMix4Stereo);
	p->addModel(modelVenomVCOUnit);
#ifndef METAMODULE
	p->addModel(modelVenomBlank);
#endif
	p->addModel(modelVenomWaveFolder);
	p->addModel(modelVenomWaveMangler);
	p->addModel(modelVenomWaveMultiplier);
#ifndef METAMODULE
	// Requires Rack GUI
	p->addModel(modelVenomWidgetMenuExtender);
#endif
	p->addModel(modelVenomWinComp);
	p->addModel(modelVenomXM_OP);

	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
	Venom::readDefaultThemes();
}
