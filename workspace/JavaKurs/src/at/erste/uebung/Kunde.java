package at.erste.uebung;

public class Kunde {
    private long kundenNr;
    private String vorname;
    private String nachnname;
    private Auto fahrzeug;

    public Kunde(long kundenNr, String vorname, String nachnname, Auto fahrzeug) {
        this.kundenNr = kundenNr;
        this.vorname = vorname;
        this.nachnname = nachnname;
        this.fahrzeug = fahrzeug;
    }

    public Kunde(long kundenNr) {
        this.kundenNr = kundenNr;
    }

    public long getKundenNr() {
        return kundenNr;
    }

    public void setKundenNr(long kundenNr) {
        this.kundenNr = kundenNr;
    }

    public String getVorname() {
        return vorname;
    }

    public void setVorname(String vorname) {
        this.vorname = vorname;
    }

    public String getNachnname() {
        return nachnname;
    }

    public void setNachnname(String nachnname) {
        this.nachnname = nachnname;
    }

    public Auto getFahrzeug() {
        return fahrzeug;
    }

    public void setFahrzeug(Auto fahrzeug) {
        this.fahrzeug = fahrzeug;
    }
}
