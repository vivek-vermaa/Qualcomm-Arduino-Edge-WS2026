#!/bin/bash
# Analog mic on JMISC pins 29-33 (AMIC2 → ADC2 → TX macro → MultiMedia1)
# Tries SWR_MIC0..SWR_MIC3 until recording is non-silent

CARD=0
RATE=16000
TEST_WAV=/tmp/mic_test.wav

try_swr_mic() {
    local SMIC=$1
    echo "--- Trying TX SMIC MUX0 = $SMIC ---"

    # Reset
    amixer -c $CARD cset name='TX DEC0 MUX' 'SWR_MIC'        >/dev/null
    amixer -c $CARD cset name='TX SMIC MUX0' "$SMIC"         >/dev/null
    amixer -c $CARD cset name='ADC2 MUX' 'INP2'              >/dev/null
    amixer -c $CARD cset name='ADC2 Switch' 1                 >/dev/null
    amixer -c $CARD cset name='ADC2 Volume' 8                 >/dev/null
    amixer -c $CARD cset name='ADC2_MIXER Switch' 1           >/dev/null
    amixer -c $CARD cset name='TX_AIF1_CAP Mixer DEC0' 1     >/dev/null
    amixer -c $CARD cset name='MultiMedia1 Mixer TX_CODEC_DMA_TX_3' 1 >/dev/null

    # Record 2 seconds
    arecord -D hw:$CARD,0 -f S16_LE -r $RATE -c 1 -d 2 $TEST_WAV 2>/dev/null

    # Check if non-silent (any sample > threshold)
    NONZERO=$(python3 -c "
import wave, struct, sys
with wave.open('$TEST_WAV','r') as f:
    data = f.readframes(f.getnframes())
samples = struct.unpack('<' + 'h'*(len(data)//2), data)
loud = sum(1 for s in samples if abs(s) > 500)
print(loud)
" 2>/dev/null)

    echo "  Non-silent samples: $NONZERO"
    if [ "${NONZERO:-0}" -gt 100 ]; then
        echo "  SUCCESS: Signal detected on $SMIC"
        return 0
    fi
    return 1
}

# Try each SWR_MIC in order
for MIC in SWR_MIC0 SWR_MIC1 SWR_MIC2 SWR_MIC3; do
    if try_swr_mic "$MIC"; then
        echo ""
        echo "Working SWR_MIC: $MIC"
        echo "Record with:"
        echo "  arecord -D hw:0,0 -f S16_LE -r 16000 -c 1 -d 5 output.wav"
        exit 0
    fi
done

echo "No signal found on SWR_MIC0-3. Check physical wiring."
echo "Make sure mic+ is on JMISC 29, micbias on JMISC 33, GND on GND."