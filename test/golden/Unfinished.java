// Copied from
// https://www.oracle.com/technical-resources/articles/hunter-meta1.html

package com.servlets;
import java.lang.annotation.*;

@Documented
@Retention(RetentionPolicy.RUNTIME)
@Target({ElementType.TYPE,ElementType.METHOD,
 ElementType.CONSTRUCTOR,ElementType.ANNOTATION_TYPE,
 ElementType.PACKAGE})
@Inherited
public @interface Unfinished {
  public enum Priority { LOW, MEDIUM, HIGH }

  String value();
  String[] owners() default "";
  Priority priority() default Priority.MEDIUM;
}
