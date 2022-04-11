/*!
 * Thanks Nigel Redmon a lot
 * Note: The corresponding file is adapted from the source code. For the source code, please refer to the following website.
 */

//
//  Biquad.h
//
//  Created by Nigel Redmon on 11/24/12
//  EarLevel Engineering: earlevel.com
//  Copyright 2012 Nigel Redmon
//
//  For a complete explanation of the Biquad code:
//  http://www.earlevel.com/main/2012/11/25/biquad-c-source-code/
//
//  License:
//
//  This source code is provided as is, without warranty.
//  You may copy and distribute verbatim copies of this document.
//  You may modify and use this source code to create binary code
//  for your own purposes, free or commercial.
//

#ifndef Biquad_h
#define Biquad_h

#include <Arduino.h>

/**
 * @enum None
 * @brief 7 filter modes
 */
enum {
    bq_type_lowpass = 0,
    bq_type_highpass,
    bq_type_bandpass,
    bq_type_notch,
    bq_type_peak,
    bq_type_lowshelf,
    bq_type_highshelf
};

class Biquad {
public:
    /**
     * @fn Biquad
     * @brief Constructor
     * @param type - Filter type select, as the enumerated type above
     * @param Fc - Ratio of filter threshold to sampling frequency, range: 0.0-0.5
     * @param Q - Filter coefficient
     * @param peakGainDB - Peak gain. It is required in some filter modes.
     * @return None
     */
    Biquad();
    Biquad(int type, float Fc, float Q, float peakGainDB);
    ~Biquad();

    /**
     * @fn setType
     * @brief Set filter type
     * @param type - Filter type select, as the enumerated type above
     * @return None
     */
    void setType(int type);

    /**
     * @fn setQ
     * @brief Set filter coefficient
     * @param Q - Filter coefficient
     * @return None
     */
    void setQ(float Q);

    /**
     * @fn setFc
     * @brief Ratio of filter threshold to sampling frequency
     * @param Fc - Ratio of filter threshold to sampling frequency, range: 0.0-0.5
     * @return None
     */
    void setFc(float Fc);

    /**
     * @fn setPeakGain
     * @brief Set peak gain
     * @param peakGainDB - Peak gain. It is required in some filter modes.
     * @return None
     */
    void setPeakGain(float peakGainDB);

    /**
     * @fn setBiquad
     * @brief Set all the parameters of the filter
     * @param type - Filter type select, as the enumerated type above
     * @param Fc - Ratio of filter threshold to sampling frequency, range: 0.0-0.5
     * @param Q - Filter coefficient
     * @param peakGainDB - Peak gain. It is required in some filter modes
     * @return None
     */
    void setBiquad(int type, float Fc, float Q, float peakGainDB);

    /**
     * @fn process
     * @brief Process the input data according to calculated parameters
     * @param in - Data to be processed
     * @return The processed data
     */
    float process(float in);
    
protected:

    /**
     * @fn calcBiquad
     * @brief Calculate the final data processing coefficient according to the parameters configured above
     * @return None
     */
    void calcBiquad(void);

    int type;
    float a0, a1, a2, b1, b2;
    float Fc, Q, peakGain;
    float z1, z2;
};

inline float Biquad::process(float in) {
    float out = in * a0 + z1;
    z1 = in * a1 + z2 - b1 * out;
    z2 = in * a2 - b2 * out;
    return out;
}

#endif // Biquad_h
