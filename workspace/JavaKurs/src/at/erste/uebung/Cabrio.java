package at.erste.uebung;

public class Cabrio extends Auto {
    protected String farbeDach;
    protected boolean dachElektrisch;

    public Cabrio(String farbe, String motorLeistung, int anzahlGaenge,
                  String farbeDach, boolean dachElektrisch) {
        super(farbe, motorLeistung, anzahlGaenge);
        this.farbeDach = farbeDach;
        this.dachElektrisch = dachElektrisch;
    }

    @Override
    public void ausgeben() {
        System.out.println("Das Cabrio hat ein Dach in"+farbeDach);
    }
}
