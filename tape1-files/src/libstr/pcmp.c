/*****************************************************************************
* pcmp - compare two pointers; return <0 if p1 < p2, 0 if p1 == p2, >0 if p1>*
* mainly used as comparison routine passed to another function which keeps   *
* elements in sorted order, when you wish to use pointer equality as test    *
*****************************************************************************/

int pcmp (p1, p2)
    char *p1, *p2;
    {
    return (p1 - p2);
    }
