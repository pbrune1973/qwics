package at.erste.uebung;

public class CompareStrings {

    public static void main(String args[]) {
        String text1 = new String("Ich bin ein String");
        String text2 = new String("Ich bin noch ein String");

        if (text1 == text2) {
            System.out.println("Gleich");
        } else {
            System.out.println("Ungleich");
        }

        if (text1.equals(text2)) {
            System.out.println("Gleich");
        } else {
            System.out.println("Ungleich");
        }
    }

}
