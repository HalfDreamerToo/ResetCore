//
// Created by 35207 on 2019/11/30 0030.
//

#ifndef RESETCORE_INT2TYPE_H
#define RESETCORE_INT2TYPE_H

// ������ӳ�䵽һ�����͵���
// ����ͨ��һ������ʱ����������̬�ַ�
template <int v>
struct Int2Type{
    enum {value = v};
};

/*
//�����ôд�Ļ�����������������Ϊһ����ֻ����һ��д��
template <typename T, bool isPolymorphic>
class NiftyContainer{
 public:
    void DoSomething(){
        T* pSomeObj = ...;
        if(isPolymorphic){
            T* pNewObj = pSomeObj->Clone();
            // Do Something
        }else{
            T* pNewObj = new T(*pSomeObj);
            // Do Something
        }
    }
};
*/

/*
template <typename T, bool isPolymorphic>
class NiftyContainer{
    void DoSomething(Int2Type<false>){

    }

    void DoSomething(Int2Type<true>){

    }
public:
    //����ֻ������õ���ģ�壬���Բ��ᱨ��
    void DoSomething(){
        DoSomething(Int2Type<isPolymorphic>);
    }
};
 */

#endif //RESETCORE_INT2TYPE_H
