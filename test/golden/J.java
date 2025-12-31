package com.example.util;

public class J {
  public static int add(int a, int b) {
    return a + b;
  }

  public static int subtract(int a, int b) {
    return a - b;
  }

  @Override
  public String toString() {
    return "I am J";
  }

  int here;  // are some
  int fields;

  class MiddleClass {
    int /* a
        multi-line
    comment */ m;

    static class InnerClass {
      static int i;

      @Override
      public String toString() {
        return "An example inner-class";
      }
    }
  }

  protected MiddleClass even;
  static Object moreFields;
}
