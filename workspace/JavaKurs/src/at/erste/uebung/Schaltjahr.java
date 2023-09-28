package at.erste.uebung;

public class Schaltjahr {
    public static void main(String args[]) {
        int jahr = 1900;

        if (((jahr % 4) == 0 && (jahr % 100) != 0) ||
                (jahr % 400 == 0)) {
            System.out.println(jahr + " ist ein Schaltjahr");
        } else {
            System.out.println(jahr + " ist kein Schaltjahr");
        }
    }
}
