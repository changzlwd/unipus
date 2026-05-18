package android.hardware.ir;

@VintfStability
interface IConsumerIr {
    boolean transmit(int carrierFrequencyHz, in int[] pattern);
    ConsumerIrFreqRange[] getCarrierFreqs();
}
