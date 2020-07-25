/*==============================================================================
DSP Effect Per Speaker Example
Copyright (c), Firelight Technologies Pty, Ltd 2004-2020.

This example shows how to manipulate a DSP network and as an example, creates 2
DSP effects, splitting a single sound into 2 audio paths, which it then filters
seperately.

To only have each audio path come out of one speaker each,
DSPConnection::setMixMatrix is used just before the 2 branches merge back together
again.

For more speakers:

 * Use System::setSoftwareFormat
 * Create more effects, currently 2 for stereo (lowpass and highpass), create one
   per speaker.
 * Under the 'Now connect the 2 effects to channeldsp head.' section, connect
   the extra effects by duplicating the code more times.
 * Filter each effect to each speaker by calling DSPConnection::setMixMatrix.
   Expand the existing code by extending the matrices from 2 in and 2 out, to the
   number of speakers you require.
==============================================================================*/
#include "fmod.hpp"
#include "common.h"

const int   INTERFACE_UPDATETIME = 50;      // 50ms update for interface
const float DISTANCEFACTOR = 1.0f;          // Units per meter.  I.e feet would = 3.28.  centimeters would = 100.

int FMOD_Main()
{
    FMOD::System* system;
    FMOD::Sound* sound;
    FMOD::Channel* channel;
    FMOD::ChannelGroup* mastergroup;
    FMOD::DSP* dsphead, * dspchannelmixer, * dspLeftDelay, * dspRightDelay;
    FMOD::DSPConnection* dspLeftDelayconnection, * dspRightDelayconnection;
    FMOD_RESULT          result;
    unsigned int         version;
    void* extradriverdata = 0;

    Common_Init(&extradriverdata);

    /*
        Create a System object and initialize.
    */
    result = FMOD::System_Create(&system);
    ERRCHECK(result);

    result = system->getVersion(&version);
    ERRCHECK(result);

    if (version < FMOD_VERSION)
    {
        Common_Fatal("FMOD lib version %08x doesn't match header version %08x", version, FMOD_VERSION);
    }

    /*
        In this special case we want to use stereo output and not worry about varying matrix sizes depending on user speaker mode.
    */
    system->setSoftwareFormat(48000, FMOD_SPEAKERMODE_STEREO, 0);
    ERRCHECK(result);

    /*
        Initialize FMOD
    */
    result = system->init(32, FMOD_INIT_NORMAL, extradriverdata);
    ERRCHECK(result);

    //Sets the global doppler scale, distance factorand log rolloff scale for all 3D sound in FMOD.
    //dopplerscale,distancefactor,rolloffscale
    result = system->set3DSettings(1.0, DISTANCEFACTOR, 1.0f);
    ERRCHECK(result);
    result = system->createSound(Common_MediaPath("car-engine1.wav"), FMOD_3D, 0, &sound);
    result = sound->set3DMinMaxDistance(0.5f * DISTANCEFACTOR, 5000.0f * DISTANCEFACTOR);//[0.5, 5000m]????
    ERRCHECK(result);
    result = sound->setMode(FMOD_LOOP_NORMAL);
    ERRCHECK(result);


    FMOD_VECTOR pos = { -10.0f * DISTANCEFACTOR, 0.0f, -10.0f * DISTANCEFACTOR };
    FMOD_VECTOR vel = { 0.0f, 0.0f, 0.0f };


    result = system->playSound(sound, 0, false, &channel);
    ERRCHECK(result);

    result = channel->set3DAttributes(&pos, &vel);
    ERRCHECK(result);

    /*
        Create the DSP effects.
    */
    //0-10ms
    //Left Delay
    result = system->createDSPByType(FMOD_DSP_TYPE_DELAY, &dspLeftDelay);
    ERRCHECK(result);
    result = dspLeftDelay->setParameterFloat(FMOD_DSP_DELAY_CH0, 1.0f);
    ERRCHECK(result);

    //RightDelay
    result = system->createDSPByType(FMOD_DSP_TYPE_DELAY, &dspRightDelay);
    ERRCHECK(result);
    result = dspRightDelay->setParameterFloat(FMOD_DSP_DELAY_CH1, 1.0f);
    ERRCHECK(result);




    /*
        Connect up the DSP network
    */

    /*
        When a sound is played, a subnetwork is set up in the DSP network which looks like this.
        Wavetable is the drumloop sound, and it feeds its data from right to left.

        [DSPHEAD]<------------[DSPCHANNELMIXER]<------------[CHANNEL HEAD]<------------[WAVETABLE - DRUMLOOP.WAV]
    */
    result = system->getMasterChannelGroup(&mastergroup);
    ERRCHECK(result);

    result = mastergroup->getDSP(FMOD_CHANNELCONTROL_DSP_HEAD, &dsphead);
    ERRCHECK(result);

    result = dsphead->getInput(0, &dspchannelmixer, 0);
    ERRCHECK(result);

    /*
        Now disconnect channeldsp head from wavetable to look like this.

        [DSPHEAD]             [DSPCHANNELMIXER]<------------[CHANNEL HEAD]<------------[WAVETABLE - DRUMLOOP.WAV]
    */
    result = dsphead->disconnectFrom(dspchannelmixer);
    ERRCHECK(result);

    /*
        Now connect the 2 effects to channeldsp head.
        Store the 2 connections this makes so we can set their matrix later.

                  [DSPLOWPASS]
                 /x
        [DSPHEAD]             [DSPCHANNELMIXER]<------------[CHANNEL HEAD]<------------[WAVETABLE - DRUMLOOP.WAV]
                 \y
                  [DSPHIGHPASS]
    */
    result = dsphead->addInput(dspLeftDelay, &dspLeftDelayconnection);      /* x = dsplowpassconnection */
    ERRCHECK(result);
    result = dsphead->addInput(dspRightDelay, &dspRightDelayconnection);    /* y = dsphighpassconnection */
    ERRCHECK(result);

    /*
        Now connect the channelmixer to the 2 effects

                  [DSPLOWPASS]
                 /x          \
        [DSPHEAD]             [DSPCHANNELMIXER]<------------[CHANNEL HEAD]<------------[WAVETABLE - DRUMLOOP.WAV]
                 \y          /
                  [DSPHIGHPASS]
    */
    result = dspLeftDelay->addInput(dspchannelmixer);     /* Ignore connection - we dont care about it. */
    ERRCHECK(result);

    result = dspRightDelay->addInput(dspchannelmixer);    /* Ignore connection - we dont care about it. */
    ERRCHECK(result);

    /*
        Now the drumloop will be twice as loud, because it is being split into 2, then recombined at the end.
        What we really want is to only feed the dsphead<-dsplowpass through the left speaker for that effect, and
        dsphead<-dsphighpass to the right speaker for that effect.
        We can do that simply by setting the pan, or speaker matrix of the connections.

                  [DSPLOWPASS]
                 /x=1,0      \
        [DSPHEAD]             [DSPCHANNELMIXER]<------------[CHANNEL HEAD]<------------[WAVETABLE - DRUMLOOP.WAV]
                 \y=0,1      /
                  [DSPHIGHPASS]
    */
    {
        float LeftDelaymatrix[2][2] = {
                                        { 1.0f, 0.0f },     // <- output to front left.  Take front left input signal at 1.0.
                                        { 0.0f, 0.0f }      // <- output to front right.  Silence
        };
        float RightDelaymatrix[2][2] = {
                                        { 0.0f, 0.0f },     // <- output to front left.  Silence
                                        { 0.0f, 1.0f }      // <- output to front right.  Take front right input signal at 1.0
        };

        /*
            Upgrade the signal coming from the channel mixer from mono to stereo.  Otherwise the lowpass and highpass will get mono signals
        */
        result = dspchannelmixer->setChannelFormat(0, 0, FMOD_SPEAKERMODE_STEREO);
        ERRCHECK(result);

        /*
            Now set the above matrices.
        */
        result = dspLeftDelayconnection->setMixMatrix(&LeftDelaymatrix[0][0], 2, 2);
        ERRCHECK(result);
        result = dspRightDelayconnection->setMixMatrix(&RightDelaymatrix[0][0], 2, 2);
        ERRCHECK(result);
    }

    result = dspLeftDelay->setBypass(true);
    ERRCHECK(result);
    result = dspRightDelay->setBypass(true);
    ERRCHECK(result);

    result = dspLeftDelay->setActive(true);
    ERRCHECK(result);
    result = dspRightDelay->setActive(true);
    ERRCHECK(result);

    /*
        Main loop.
    */
    do
    {
        bool LeftDelayBypass, RightDelayBypass;

        Common_Update();

        result = dspLeftDelay->getBypass(&LeftDelayBypass);
        ERRCHECK(result);
        result = dspRightDelay->getBypass(&RightDelayBypass);
        ERRCHECK(result);

        if (Common_BtnPress(BTN_ACTION1))
        {
            LeftDelayBypass = !LeftDelayBypass;

            result = dspLeftDelay->setBypass(LeftDelayBypass);
            ERRCHECK(result);
        }

        if (Common_BtnPress(BTN_ACTION2))
        {
            RightDelayBypass = !RightDelayBypass;

            result = dspRightDelay->setBypass(RightDelayBypass);
            ERRCHECK(result);
        }


        if (Common_BtnDown(BTN_LEFT))
        {
            pos.x -= 1.0f * DISTANCEFACTOR;
            result = channel->set3DAttributes(&pos, &vel);
            ERRCHECK(result);
        }

        if (Common_BtnDown(BTN_RIGHT))
        {
            pos.x += 1.0f * DISTANCEFACTOR;
            result = channel->set3DAttributes(&pos, &vel);
            ERRCHECK(result);
        }

        if (Common_BtnDown(BTN_UP))
        {
            pos.z += 1.0f * DISTANCEFACTOR;
            result = channel->set3DAttributes(&pos, &vel);
            ERRCHECK(result);
        }

        if (Common_BtnDown(BTN_DOWN))
        {
            pos.z -= 1.0f * DISTANCEFACTOR;
            result = channel->set3DAttributes(&pos, &vel);
            ERRCHECK(result);
        }


        result = system->update();
        ERRCHECK(result);

        Common_Draw("");
        Common_Draw("Press %s to toggle left delay", Common_BtnStr(BTN_ACTION1));
        Common_Draw("Press %s to toggle right delay", Common_BtnStr(BTN_ACTION2));
        Common_Draw("Press %s or %s to pan sound", Common_BtnStr(BTN_LEFT), Common_BtnStr(BTN_RIGHT));
        Common_Draw("Press %s to quit", Common_BtnStr(BTN_QUIT));
        Common_Draw("");
        Common_Draw("LeftDelay is %s", LeftDelayBypass ? "inactive" : "active");
        Common_Draw("RightDelay is %s", RightDelayBypass ? "inactive" : "active");
        Common_Draw("Pan is %0.2f", pos.x);
        Common_Draw("Up is %0.2f", pos.y);
        Common_Draw("Forward is %0.2f", pos.z);


        Common_Sleep(50);
    } while (!Common_BtnPress(BTN_QUIT));

    /*
        Shut down
    */
    result = sound->release();
    ERRCHECK(result);

    result = dspLeftDelay->release();
    ERRCHECK(result);
    result = dspRightDelay->release();
    ERRCHECK(result);

    result = system->close();
    ERRCHECK(result);
    result = system->release();
    ERRCHECK(result);

    Common_Close();

    return 0;
}
