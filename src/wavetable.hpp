#ifndef __WAVETABLE_HPP__
#define __WAVETABLE_HPP__

#include "util/common.hpp"

#define TABLE_SIZE 2048 

namespace rack {

  enum EnvType {
    SIN,
    TRI,
    HANN,
    WELCH,
    TUKEY,
    NUM_ENVS
  };

  struct Wavetable {

    float table[TABLE_SIZE];
    
    EnvType et;

    Wavetable() {
      // default to a cycle of a sin wave
      et = SIN;
      initSinWav(); 
    }

    Wavetable(EnvType e) {
      et = e;
      init(e);
    }

    void init(EnvType e) {
      switch (e) {
        case SIN:
          initSinWav();
          break;
        case TRI:
          initDefaultEnv();
          break;
        case HANN:
          initHannEnv();
          break;
        case WELCH:
          initWelchEnv();
          break;
        case TUKEY:
          initTukeyEnv();
          break;
        default:
          initSinWav();
      }
    }

    void switchEnvType(EnvType e) {
      // don't switch if already that env
      if (et != e) {
        et = e;
        init(e); 
      }
    }

    void initSinWav() {
      // Fill the wavetable
      float phase = 0.f;
      for (int i=0; i<TABLE_SIZE; i++) {
        table[i] = sinf(2.f*M_PI * phase); 
        phase += (float) i  / (2.f*M_PI);
      }
    }

    void initDefaultEnv() {
      float phase = 0.f;
      for (int i=0; i<TABLE_SIZE; i++) {
        // TODO
        // make env table
        if (phase < 0.5f) table[i] = (float) i / TABLE_SIZE;
        else table[i] = ((-1.f * i) / TABLE_SIZE) + 2.f;
        phase += 1.f / TABLE_SIZE;
      }
    }

    void initHannEnv() {
      float a_0 = 0.5f;
      for (int i=0; i<TABLE_SIZE; i++) {
        table[i] = a_0 * (1 - cosf((2.f * M_PI * ((float) i / TABLE_SIZE)) / 1.f));
      }
    }

    void initWelchEnv() {
      float ts = (float) TABLE_SIZE;
      for (int i=0; i<TABLE_SIZE; i++) {
        table[i] = 1.f - pow((((float) i / ts) - (ts / 2.f)) / (ts / 2.f), 2); 
      }
    }

    void initTukeyEnv() {
      float p1,p2,N,alpha,n;

      alpha = 0.5f;

      N = (float) TABLE_SIZE;
      p1 = alpha * N / 2;
      p2 = N * (1 - (alpha / 2));

      for (int i=0; i<TABLE_SIZE; i++) {
        n = (float) i / N;
        if (n < p1) {
          table[i] = 0.5f * (1 + cosf(M_PI * (((2 * n) / (alpha * N)) - 1)));
        }
        else if (n <= p2) table[i] = 1.f; 
        else {
          table[i] = 0.5f * (1 + cosf(M_PI * (((2 * n) / (alpha * N)) - (2 / alpha) + 1)));
        }
      }
    }

    float operator[](int x) {
      return table[x];
    }

    float operator[](float x) {
      return index(x); 
    }

    float index(float x) {
      float fl = floorf(x);
      float ph = x - fl;
      float lb = table[(int) fl];
      float ub = table[(int) ceilf(x)];

      return ((1.0 - ph) * lb) + (ph * ub);
    }

    /*
     * Expects val 0.0 <= x < 1.0
     */
    float get(float x) {
      return index(x * (float) TABLE_SIZE); 
    }
  };

}

#endif
