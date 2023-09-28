package at.erste.uebung;

public class Fehler {

    public static double divide(int x, int y) throws Exception {
/*
        if (y == 0) {
            throw new IllegalArgumentException("Division durch 0");
        }
        */
        return x/y;
    }

    public static void main(String args[]) {
        String eingabe = "xxx";
        try {
            int jahr = Integer.parseInt(eingabe);
            System.out.println(jahr);
        } catch (Exception e) {
            e.printStackTrace();
        }

        try {
            double d = divide(5, 0);
            System.out.println(d);
            d = d * 100;
        } catch (Exception e) {
            e.printStackTrace();
        }
        System.out.println("Ende");
    }
}
