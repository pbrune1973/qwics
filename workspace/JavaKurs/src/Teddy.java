public class Teddy {
    private String farbe;
    private double groesse;

    public Teddy(String farbe, double groesse) {
        this.farbe = farbe;
        this.groesse = groesse;
    }

    public void druecken() {
        System.out.println("brumm");
    }

    public static void main(String args[]) {
        Teddy t1 = new Teddy("braun",15.0);
        t1.druecken();

        Teddy t2 = new Teddy("beige",20.0);
        t2.druecken();
    }
}
