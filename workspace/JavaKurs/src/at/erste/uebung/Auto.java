package at.erste.uebung;

public class Auto {
    protected String farbe;
    protected String motorLeistung;
    protected int anzahlGaenge;

    public void setFarbe(String farbe) {
        this.farbe = farbe;
    }

    public void setFarbe(int farbe) {
        if (farbe == 1) {
            this.farbe = "rot";
        }
    }

    public String getFarbe() {
        return farbe;
    }

    public Auto(String farbe, String motorLeistung, int anzahlGaenge) {
        this.farbe = farbe;
        this.motorLeistung = motorLeistung;
        this.anzahlGaenge = anzahlGaenge;
    }

    public void ausgeben() {
        System.out.println("Das Auto ist "+farbe+" und macht tuut.");
    }




    public static void main(String args[]) {
        Auto a = new Auto("rot","100",5);
        a.ausgeben();
        Kunde k1 = new Kunde(1234);

        a = new Cabrio("silber","200",6,"schwarz",true);
        a.ausgeben();
        k1.setFahrzeug(a);
    }
}
