//
// Created by 35207 on 2019/11/30 0030.
//

#ifndef RESETCORE_STATICASSERT_H
#define RESETCORE_STATICASSERT_H

template <bool> struct CompileTimeError{
    CompileTimeError(...); //���������
};
template <> struct CompileTimeError<false>{};

// �����ERROR_##msg������ʽ��ʾ������Ϣ
// ʵ�ʵ�STATIC_CHECK��
#define STATIC_CHECK(expr, msg) \
    { \
        class ERROR_##msg {}; \
        (void)sizeof(CompileTimeChecker<(expr)>(ERROR_##msg()));\
    }

#endif //RESETCORE_STATICASSERT_H
