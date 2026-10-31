#pragma once
#include "Panel.h"
#include "../PluginProcessor.h"

class MeterPanel : public Panel, private juce::Timer {
public:
    MeterPanel(Skin& s, HarmonicCompressorAudioProcessor& p, bool inputOnly = false)
      : Panel(s, inputOnly ? "INPUT" : "OUTPUT / GR"), proc(p), input(inputOnly) { startTimerHz(30); }
    void paint(juce::Graphics& g) override {
        Panel::paint(g);
        const int lanes = input ? 2 : 3;
        const float top = 70, bottom = float(getHeight()-48), height = bottom-top;
        const float laneWidth = input ? 12.0f : 14.0f;
        const float left = input ? 12.0f : 14.0f;
        const float values[] = {input ? proc.peakInputDbL.load() : proc.peakOutputDbL.load(),
                                input ? proc.peakInputDbR.load() : proc.peakOutputDbR.load(),
                                juce::jlimit(0.0f, 36.0f, -proc.lastGrDbForUI.load())};
        for (int i=0;i<lanes;++i) {
            const float x = left + float(i)*(laneWidth+5) + (i==2 ? 28 : 0);
            g.setColour(juce::Colour(0xffbecfdd)); g.fillRect(x,top,laneWidth,height);
            const float proportion = i==2 ? values[i]/36.0f : juce::jlimit(0.0f,1.0f,(values[i]+60)/60);
            const auto colour = i==2 ? juce::Colour(0xffc48728) : juce::Colour(0xff297ea6);
            g.setColour(colour);
            g.fillRect(x, i==2 ? top : bottom-height*proportion, laneWidth, height*proportion);
            g.setColour(juce::Colour(0xff35536b)); g.setFont(juce::FontOptions(9.0f));
            g.drawText(i==2 ? "GR" : i==0 ? "L" : "R", int(x)-3,int(bottom)+8,int(laneWidth)+6,14,juce::Justification::centred);
        }
        g.setFont(juce::FontOptions(8.0f));
        for(int db=0;db<=60;db+=12) {
            const float y=top+height*float(db)/60;
            g.setColour(juce::Colour(0xff7692a9));
            g.drawHorizontalLine(int(y),left,left+laneWidth*2+5);
            g.drawText(juce::String(-db), int(left+laneWidth*2+7),int(y)-6,24,12,juce::Justification::centredLeft);
        }
        g.setColour(juce::Colour(0xff35536b)); g.setFont(juce::FontOptions(10.0f,juce::Font::bold));
        g.drawText(juce::String(input ? proc.lastInputPeakDb.load() : proc.lastOutputPeakDb.load(),1)+" dBFS",0,getHeight()-23,getWidth(),15,juce::Justification::centred);
        if(!input) { g.setColour(juce::Colour(0xffa16e20)); g.drawText(juce::String(values[2],1)+" dB GR",0,43,getWidth(),15,juce::Justification::centred); }
    }
private:
    void timerCallback() override { if(isShowing()) repaint(); }
    HarmonicCompressorAudioProcessor& proc;
    bool input;
};

class CompressionScope : public Panel, private juce::Timer {
public:
    CompressionScope(Skin& s,HarmonicCompressorAudioProcessor& p)
      : Panel(s,"COMPRESSION HISTORY", "INPUT / OUTPUT ENVELOPES + APPLIED GR   /   6 SECONDS"),proc(p) {
        // Discard stale telemetry accumulated before this editor was opened.
        proc.historyFifo.finishedRead(proc.historyFifo.getNumReady());
        startTimerHz(30);
    }
    void paint(juce::Graphics& g) override {
        Panel::paint(g);
        auto r=getLocalBounds().toFloat().reduced(16).withTrimmedTop(38).withTrimmedBottom(16);
        g.setColour(juce::Colour(0xffe8f0f6)); g.fillRect(r);
        g.setColour(juce::Colour(0xffc5d6e4));
        for(int i=0;i<=6;++i) {float x=r.getX()+r.getWidth()*float(i)/6;g.drawVerticalLine(int(x),r.getY(),r.getBottom());}
        for(int i=0;i<=4;++i) g.drawHorizontalLine(int(r.getY()+r.getHeight()*float(i)/4),r.getX(),r.getRight());
        juce::Path gr;
        bool first=true; float elapsed=0;
        for(int i=0;i<count;++i) {
            const auto& f=frames[(size_t)((write-1-i+capacity)%capacity)];
            const float x=r.getRight()-elapsed/6*r.getWidth();
            if(x<r.getX()) break;
            const float width=juce::jmax(1.0f,f.seconds/6*r.getWidth());
            // A symmetric peak envelope, not an invented sample waveform.
            auto amplitude=[](float peak){return juce::jlimit(0.0f,1.0f,(juce::Decibels::gainToDecibels(peak,-60.0f)+60)/60);};
            for(int layer=0;layer<2;++layer) {
                const float h=amplitude(layer==0?f.input:f.output)*r.getHeight()*0.42f;
                g.setColour(layer==0?juce::Colour(0xff9ebbd0):juce::Colour(0xff4389ae).withAlpha(0.8f));
                g.fillRect(juce::jmax(r.getX(),x-width),r.getCentreY()-h,juce::jmin(width,x-r.getX()),2*h);
            }
            const float y=r.getY()+juce::jlimit(0.0f,36.0f,f.reduction)/36*r.getHeight();
            if(first){gr.startNewSubPath(x,y);first=false;}else gr.lineTo(x,y);
            elapsed+=f.seconds;
        }
        g.setColour(juce::Colour(0xffb5761c));g.strokePath(gr,juce::PathStrokeType(2.0f));
        g.setFont(juce::FontOptions(10.0f));
        g.drawText("GR  0 - 36 dB",r.withHeight(15).reduced(5,0),juce::Justification::topLeft);
        g.setColour(juce::Colour(0xff35536b));
        if(count==0) g.drawText("Waiting for audio",r,juce::Justification::centred);
        g.drawText("-6 s                                           INPUT      OUTPUT      GR                                           NOW",16,getHeight()-24,getWidth()-32,15,juce::Justification::centred);
    }
private:
    void timerCallback() override {
        if(proc.historyDrops.exchange(0)>0) { count=0;write=0;pending={}; }
        int a,n,b,m;proc.historyFifo.prepareToRead(proc.historyFifo.getNumReady(),a,n,b,m);
        auto append=[&](int start,int size){for(int i=0;i<size;++i){
            const auto f=proc.historyFrames[(size_t)(start+i)];
            // Aggregate audio blocks into >=10ms display bins, preserving peaks.
            pending.input=juce::jmax(pending.input,f.input);pending.output=juce::jmax(pending.output,f.output);
            pending.reduction=juce::jmax(pending.reduction,f.reduction);pending.seconds+=f.seconds;
            if(pending.seconds>=0.01f){frames[(size_t)write]=pending;write=(write+1)%capacity;count=juce::jmin(capacity,count+1);pending={};}
        }};
        append(a,n);append(b,m);proc.historyFifo.finishedRead(n+m);repaint();
    }
    static constexpr int capacity=640;
    HarmonicCompressorAudioProcessor& proc;
    std::array<HarmonicCompressorAudioProcessor::HistoryFrame,capacity> frames{};
    HarmonicCompressorAudioProcessor::HistoryFrame pending{};
    int write=0,count=0;
};
