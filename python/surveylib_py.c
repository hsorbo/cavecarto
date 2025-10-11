#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "cavecarto.h"
#include <datetime.h>

static PyObject* py_get_coords(PyObject *self, PyObject *args) {
    PyObject *stations_list;
    PyObject *datum_dict;
    int flags;
    int latlon;

    if (!PyArg_ParseTuple(args, "OOii", &stations_list, &datum_dict, &flags, &latlon))
        return NULL;

    Py_ssize_t n = PyList_Size(stations_list);
    CCStation *stations = calloc(n, sizeof(CCStation));
    if (!stations) return PyErr_NoMemory();

    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject *entry = PyList_GetItem(stations_list, i);
        if (!PyDict_Check(entry)) {
            PyErr_SetString(PyExc_TypeError, "'stations' must be a list");
            goto error;
        }

        PyObject *depth_obj = PyDict_GetItemString(entry, "depth");
        if (!depth_obj) {
            PyErr_SetString(PyExc_TypeError, "'depth' must be a float");
            goto error;
        }
        double depth = PyFloat_AsDouble(depth_obj);

        PyObject *date_obj = PyDict_GetItemString(entry, "date");

        if (!date_obj || !PyDateTime_Check(date_obj)) {
            PyErr_SetString(PyExc_TypeError, "Station.date must be a datetime.datetime object");
            goto error;
        }

        PyObject *location_obj = PyDict_GetItemString(entry, "location");
        PyObject *distance_obj = PyDict_GetItemString(entry, "distance");

        if (location_obj && PyDict_Check(location_obj)) {
            PyObject *lat = PyDict_GetItemString(location_obj, "lat");
            PyObject *lon = PyDict_GetItemString(location_obj, "lon");
            if (!lat || !lon) {
                PyErr_SetString(PyExc_ValueError, "lat/lon must be set");
                goto error;
            }

            cc_station_init_fixed(
                &stations[i],
                -1,
                PyFloat_AsDouble(lat),
                PyFloat_AsDouble(lon),
                depth);
                
        } else if (distance_obj && PyDict_Check(distance_obj)) {
            
            PyObject *parent_index_str = PyDict_GetItemString(distance_obj, "parent_index");
            if (!parent_index_str) {
                PyErr_SetString(PyExc_TypeError, "'parent_index' must be set");
                goto error;
            }
            if (!PyLong_Check(parent_index_str)) {
                PyErr_SetString(PyExc_TypeError, "'parent_index' must be an int");
                goto error;
            }
            int parent_index = (int)PyLong_AsLong(parent_index_str);
            if (parent_index > n) {
                PyErr_SetString(PyExc_ValueError, "'parent_index' out of bounds");
                goto error;
            }
            
            ssize_t closure_index = -1;
            PyObject *closed_index_str = PyDict_GetItemString(distance_obj, "closed_index");
            if (closed_index_str && closed_index_str != Py_None) {
                if (!PyLong_Check(closed_index_str)) {
                    PyErr_SetString(PyExc_TypeError, "'closed_index' must be an int");
                    goto error;
                }
                int closed_index = (int)PyLong_AsLong(closed_index_str);
                if (closed_index > n) {
                    PyErr_SetString(PyExc_ValueError, "'closed_index' out of bounds");
                    goto error;
                }
                closure_index = closed_index;
            }

            PyObject *az = PyDict_GetItemString(distance_obj, "azimuth");
            if(!az) {
                PyErr_SetString(PyExc_ValueError, "azimuth must be set");
                goto error;
            }
            
            PyObject *len = PyDict_GetItemString(distance_obj, "length");
            if (!len) {
                PyErr_SetString(PyExc_ValueError, "length must be set");
                goto error;
            }

            CCDate date = {
                .year = PyDateTime_GET_YEAR(date_obj),
                .month = PyDateTime_GET_MONTH(date_obj),
                .day = PyDateTime_GET_DAY(date_obj)
            };

            cc_station_init_relative(
                &stations[i],
                parent_index,
                closure_index,
                PyFloat_AsDouble(az),
                PyFloat_AsDouble(len),
                depth,
                date);

        } else {
            PyErr_SetString(PyExc_ValueError, "Station must contain 'location' or 'distance'");
            goto error;
        }
    }

    CCLocation datum;
    CCLocation *datum_ptr = NULL;

    if (datum_dict != Py_None) {
        if (!PyDict_Check(datum_dict)) goto error;
        PyObject *lat = PyDict_GetItemString(datum_dict, "lat");
        PyObject *lon = PyDict_GetItemString(datum_dict, "lon");
        if (!lat || !lon) goto error;
        datum.lat = PyFloat_AsDouble(lat);
        datum.lon = PyFloat_AsDouble(lon);
        datum_ptr = &datum;
    }
    

    CCPoint *pos = calloc(n, sizeof(CCPoint));
    if (!pos) goto error;
    CCComputeResult res = latlon 
        ? cc_compute_station_latlon(stations, n, datum_ptr, (CCLocation*)pos, flags)
        : cc_compute_station_xy(stations, n, datum_ptr, pos, flags);
    if (res != CCStation_SUCCESS) {
        switch (res)
        {
        case CCStation_NO_DATUM:
            PyErr_SetString(PyExc_RuntimeError, "No datum (fixed station) found");
            break;
        case CCStation_CYCLE_DETECTED:
            PyErr_SetString(PyExc_RuntimeError, "Cycles in stations");
            break;
        case CCStation_NULL:
            PyErr_SetString(PyExc_RuntimeError, "Station is NULL");
            break;
        case CCStation_RELATIVE_MISSING_PARENT:
            PyErr_SetString(PyExc_RuntimeError, "Relative station missing parent");
            break;
        default:
            PyErr_SetString(PyExc_RuntimeError, "Error computing");
            break;
        }
        
        goto error;
    }

    PyObject *coord_list = PyList_New(n);
    if (!coord_list) goto error;
    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject *x = PyFloat_FromDouble(pos[i].x);
        PyObject *y = PyFloat_FromDouble(pos[i].y);
        if (!x || !y) goto error;
        PyObject *pair = PyTuple_Pack(2, x, y);
        Py_DECREF(x);
        Py_DECREF(y);
        if (!pair) goto error;
        PyList_SET_ITEM(coord_list, i, pair);
    }

    free(stations); 
    free(pos); 
    return coord_list;

error:
    free(stations);
    return NULL;
}


static PyMethodDef SurveylibMethods[] = {
    {"get_coords", py_get_coords, METH_VARARGS, "Compute coordinates from stations"},
    {NULL, NULL, 0, NULL}  // Sentinel
};

static struct PyModuleDef surveylib_module = {
    PyModuleDef_HEAD_INIT,
    "_surveylib",
    NULL,
    -1,
    SurveylibMethods
};

PyMODINIT_FUNC PyInit__surveylib(void) {
    PyDateTime_IMPORT;
    return PyModule_Create(&surveylib_module);
}
