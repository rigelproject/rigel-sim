// NOTE: (LONG LINES)  All lines wrap at < 100 characters.

// NOTE: (FILE HEADERS) At the top of every file should be a header like the one
//       below with the name of the file and a description of what the file
//       contains.

////////////////////////////////////////////////////////////////////////////////
// style_guide.h
////////////////////////////////////////////////////////////////////////////////
//
//  This file contains the definition of the style guide class for use in the
//  style guide example for coding conventions within RigelSim.
//
////////////////////////////////////////////////////////////////////////////////

// NOTE:  (INCLUDE START) All includes should have the #ifndef lines to make sure
//        that there are no double-includes.
#ifndef __STYLE_GUIDE_H__
#define __STYLE_GUIDE_H__

// NOTE: (CLASS HEADERS) All major classes should have full headers that name
//       the class, describe it briefly, and list any initialization or teardown
//       issues that a user may need to be aware of.  

////////////////////////////////////////////////////////////////////////////////
/// StyleGuide
///  
///  This is an example class used for the RigelSim style guide.
////////////////////////////////////////////////////////////////////////////////
class StyleGuide
{
  // NOTE: (MEMBER CLASSIFICATION) All members should be explicitly tagged as
  //       public/private/protected.  There should be separate blocks for each
  //       type and for member functions versus member data.
  
  // public member functions
  public:
    
    // NOTE: (SINGLE LINE FUNCTIONS) Single-line functions are appropriate when
    //       it does not cause a line wrap to occur.

    // Default constructor.
    StyleGuide() { count = 0; }

    // NOTE: (UNIMPLEMENTED MEMBERS) Always explicitly note when a member is not
    //       implemented on purpose.

    // Constructor not implemented on purpose.
    StyleGuide(int i);

    // NOTE: (ACCESSORS) Small accessor members need not be fully documented.
    //       Their function should be noted and any non-obvious parameters or
    //       return values should be documented.

    // NOTE: (CONST PARAMETERS)  Any time that an input to a member is not
    //       modified, it should be tagged as 'const'.  Not only does this
    //       provide stricter static type checking, but it also ensures that the
    //       methods will play nicely with const inputs and other libraries.
   
    // Add to the count.
    void add(const int val) { count += val; }
    // Bogus valid check.  Returns true if it feels like it.  This function has
    // no real meaning other than as an example.
    // NOTE: (CONST KEYWORD)  If a method does not modify any data, then declear
    //       it const. If the inputs are not modified, they should be const as
    //       well.  This is especially true for references.
    bool valid(const int val) const;
    void remove(const int val) { count -= val; }

  // public data members
  public:

  // protected member functions
  protected: 

  // protected data members
  protected:

  // private member functions
  private: 
    bool always_true() { return true; }

  // private data members
  private:
    int count;
};

////////////////////////////////////////////////////////////////////////////////
// NOTE: (LONG MEMBERS) Members functions that are longer than a couple of lines
//       should be moved into the same file with the 'inline' keyword (or into
//       the implementation file).
// (header goes here)
////////////////////////////////////////////////////////////////////////////////
inline bool 
StyleGuide::valid(const int val) const
{
  if (val > count) return true;
  if (val == count) { count = 0; }
  // Default is to return false.
  return false;
}
#endif
