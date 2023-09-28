package at.erste.uebung;

import java.util.ArrayList;

public class Maximum {
    public static void main(String args[]) {
        /*
        int zahlen[] = new int[] { 3, -7, 5, 2, 0 };
        int max = zahlen[0];

        for (int i = 0; i < zahlen.length; i++) {
            if (zahlen[i] > max) {
                max = zahlen[i];
            }
        }
*/
        ArrayList<Integer> zahlen = new ArrayList();
        zahlen.add(7);
        zahlen.add(5);
        zahlen.add(7);
        zahlen.add(8);
        zahlen.add(9);
        int max = zahlen.get(0);

        for (int z : zahlen) {
            if (z > max) {
                max = z;
            }
        }

        System.out.println(max);
    }
}
