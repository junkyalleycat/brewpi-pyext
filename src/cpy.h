// This class makes handling local python objects easier
// at it moves their decref to the destructor           
typedef struct {} NewRef_PyObject;                      
                                                        
class CPyObject {                                       
                                                        
    public:                                             
        virtual ~CPyObject() {};                        
        virtual operator PyObject *() = 0;              
        virtual operator NewRef_PyObject *() = 0;       
                                                        
};                                                      
                                                        
class PPyObject: public CPyObject {                     
    private:                                            
        PyObject *o;                                    
    public:                                             
        PPyObject(PyObject *o) {                        
            if(o == NULL) {                             
                throw std::exception();                 
            }                                           
            this->o = o;                                
        }                                               
        ~PPyObject() {                                  
            Py_XDECREF(o);                              
        }                                               
        operator PyObject *() {                         
            return o;                                   
        }                                               
        operator NewRef_PyObject *() {                  
            Py_XINCREF(o);                              
            return (NewRef_PyObject *) o;               
        }                                               
};                                                      
                                                        
class UPyObject: public CPyObject {                     
    private:                                            
        PyObject *o;                                    
    public:                                             
        UPyObject(PyObject *o) {                        
            if(o == NULL) {                             
                throw std::exception();                 
            }                                           
            Py_XINCREF(o);                              
            this->o = o;                                
        }                                               
        ~UPyObject() {                                  
            Py_XDECREF(o);                              
        }                                               
        operator PyObject *() {                         
            return o;                                   
        }                                               
        operator NewRef_PyObject *() {                  
            Py_XINCREF(o);                              
            return (NewRef_PyObject *) o;               
        }                                               
};                                                      
