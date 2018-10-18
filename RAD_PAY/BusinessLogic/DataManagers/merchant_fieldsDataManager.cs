using RAD_PAY.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class merchant_fieldsDataManager
    {
//merchant_fieldsViewModel
//publicintfid{get;set;}
//publicint?mid{get;set;}
//publicstringlabel{get;set;}
//publicint?type{get;set;}
//publicint?input_digit{get;set;}
//publicint?position{get;set;}
//publicint?input_letter{get;set;}
//publicstringprefix_label{get;set;}
//publicstringlabel_uz1{get;set;}
//publicstringlabel_uz2{get;set;}
//publicint?min_length{get;set;}
//publicint?max_length{get;set;}
//publicint?parent_fid{get;set;}
//publicstringparam_name{get;set;}
//publicint?usage{get;set;}

        public static void Add(merchant_fieldsViewModel model, RAD_PAYEntities db)
        {
            var dbmodel = new merchant_fields
            {
                fid         = model.fid             ,    
                mid         = model.mid             ,
                label       = model.label           ,
                type        = model.type            ,
                input_digit = model.input_digit     ,
                position    = model.position        ,
                input_letter= model.input_letter    ,
                prefix_label= model.prefix_label    ,
                label_uz1   = model.label_uz1       ,
                label_uz2   = model.label_uz2       ,
                min_length  = model.min_length      ,
                max_length  = model.max_length      ,
                parent_fid  = model.parent_fid      ,
                param_name  = model.param_name      ,
                usage       = model.usage           ,
            };

            db.merchant_fields.Add(dbmodel);
        }

        public static void Modify(merchant_fieldsViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchant_fields.Where(z => z.fid == model.fid);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    dbmodel.fid = model.fid             ;    
                    dbmodel.mid = model.mid             ;
                    dbmodel.label = model.label           ;
                    dbmodel.type = model.type            ;
                    dbmodel.input_digit = model.input_digit     ;
                    dbmodel.position = model.position        ;
                    dbmodel.input_letter = model.input_letter    ;
                    dbmodel.prefix_label = model.prefix_label    ;
                    dbmodel.label_uz1 = model.label_uz1       ;
                    dbmodel.label_uz2 = model.label_uz2       ;
                    dbmodel.min_length = model.min_length      ;
                    dbmodel.max_length = model.max_length      ;
                    dbmodel.parent_fid = model.parent_fid      ;
                    dbmodel.param_name = model.param_name      ;
                    dbmodel.usage = model.usage           ;
                }
            }
        }

        public static void Delete(merchant_fieldsViewModel model, RAD_PAYEntities db)
        {
            var result = db.merchant_fields.Where(z => z.fid == model.fid);

            if (result.Any())
            {
                var dbmodel = result.FirstOrDefault();

                if (dbmodel != null)
                {
                    db.merchant_fields.Remove(dbmodel);
                }
            }
        }

        public static List<merchant_fieldsViewModel> Get(merchant_fieldsViewModel model, RAD_PAYEntities db)
        {
            List<merchant_fieldsViewModel> list = null;

            var query = from resmodel in db.merchant_fields
                        select new merchant_fieldsViewModel
                        {
                            fid = resmodel.fid,
                            mid = resmodel.mid,
                            label = resmodel.label,
                            type = resmodel.type,
                            input_digit = resmodel.input_digit,
                            position = resmodel.position,
                            input_letter = resmodel.input_letter,
                            prefix_label = resmodel.prefix_label,
                            label_uz1 = resmodel.label_uz1,
                            label_uz2 = resmodel.label_uz2,
                            min_length = resmodel.min_length,
                            max_length = resmodel.max_length,
                            parent_fid = resmodel.parent_fid,
                            param_name = resmodel.param_name,
                            usage = resmodel.usage,
                        };

            list = query.ToList();

            return list;
        }
    }
}