public class Rechner {
    public static final double ZINS_SATZ = 1.1;

    public static void main(String args[]) {
        double x = 5.0;
        x = x * ERSTEConfig.ZINS_SATZ;
        System.out.println(x);
    }
}
